/*=========================================================================

Program:   Visualization Toolkit
Module:    vtkKakaduReader.cxx

Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
All rights reserved.
See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

This software is distributed WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.  See the above copyright notice for more information.

=========================================================================*/

#include "vtkKakaduReader.h"
#include "vtkCallbackCommand.h"
#include "vtkDataArraySelection.h"
#include "vtkExtentTranslator.h"
#include "vtkUnsignedIntArray.h"
#include "vtkGridSampler1.h"
#include "vtkImageData.h"
#include "vtkInformation.h"
#include "vtkInformationVector.h"
#include "vtkIntArray.h"
#include "vtkMetaInfoDatabase.h"
#include "vtkObjectFactory.h"
#include "vtkPointData.h"
#include "vtkSmartPointer.h"
#include "vtkStreamingDemandDrivenPipeline.h"

#include "vtk_netcdf.h"
#include <string>
#include <vector>

#include "vtkExtentTranslator.h"

// Kakadu core includes
#include <kdu_arch.h>
#include <kdu_elementary.h>
#include <kdu_messaging.h>
#include <kdu_params.h>
#include <kdu_compressed.h>
#include <kdu_sample_processing.h>
// Application includes
#include <kdu_file_io.h>
#include <jp2.h>
#include <kdu_region_decompressor.h>

#define DEBUGPRINT(arg) ;

#define DEBUGPRINT_RESOLUTION(arg) ;

#define DEBUGPRINT_METAINFORMATION(arg) ;

vtkStandardNewMacro(vtkKakaduReader);

//============================================================================

class vtkKakaduReader::Internal
{
public:
  vtkSmartPointer<vtkDataArraySelection> VariableArraySelection;
  // a mapping from the list of all variables to the list of available
  // point-based variables
  std::vector<int> VariableMap;
  Internal()
  {
    this->VariableArraySelection =
      vtkSmartPointer<vtkDataArraySelection>::New();

    this->GridSampler = vtkGridSampler1::New();
    this->RangeKeeper = vtkMetaInfoDatabase::New();
    this->Resolution = 1.0;
    this->SI = 1;
    this->SJ = 1;
    this->SK = 1;
    this->WholeExtent[0] =
      this->WholeExtent[2] =
      this->WholeExtent[4] = 1;
    this->WholeExtent[1] =
      this->WholeExtent[3] =
      this->WholeExtent[5] = -1;
    this->ResLevels = 1;
  }
  ~Internal()
  {
    this->GridSampler->Delete();
    this->RangeKeeper->Delete();
  }
  vtkGridSampler1 *GridSampler;
  vtkMetaInfoDatabase *RangeKeeper;
  double Resolution;
  int SI, SJ, SK;
  int WholeExtent[6];
  int ResLevels;
};

//----------------------------------------------------------------------------
//set default values
vtkKakaduReader::vtkKakaduReader()
{
  this->SetNumberOfInputPorts(0);
  this->SetNumberOfOutputPorts(1);
  this->FileName = NULL;

  this->SelectionObserver = vtkCallbackCommand::New();
  this->SelectionObserver->SetCallback
    (&vtkKakaduReader::SelectionModifiedCallback);
  this->SelectionObserver->SetClientData(this);

  this->Origin[0] = this->Origin[1] = this->Origin[2] = 0.0;
  this->Spacing[0] = this->Spacing[1] = this->Spacing[2] = 1.0;

  this->Internals = new vtkKakaduReader::Internal;
  this->Internals->VariableArraySelection->AddObserver(
    vtkCommand::ModifiedEvent, this->SelectionObserver);
}

//----------------------------------------------------------------------------
//delete filename and netcdf file descriptor
vtkKakaduReader::~vtkKakaduReader()
{
  if(this->SelectionObserver)
    {
    this->SelectionObserver->Delete();
    this->SelectionObserver = NULL;
    }
  if(this->Internals)
    {
    delete this->Internals;
    this->Internals = NULL;
    }
}

//----------------------------------------------------------------------------
void vtkKakaduReader::PrintSelf(ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "FileName: "
     << (this->FileName ? this->FileName : "(NULL)") << endl;

  this->Internals->VariableArraySelection->PrintSelf
    (os, indent.GetNextIndent());
}

//----------------------------------------------------------------------------
static void
  find_rational_scale(float scale, kdu_coords &numerator,
      kdu_coords &denominator, double min_safe_scale_prod,
      double max_safe_scale_x, double max_safe_scale_y)
  /* The min/max safe scaling factors supplied here are derived from the
     `kdu_region_decompressor' object.  In practice, the maximum safe scaling
     factor will be vastly larger than the point at which you will run out
     of memory in these simple demo apps, because they try to render the
     entire image to a buffer.  A real application should ideally use these
     to limit scaling for the case in which a region of an image is
     being rendered at a massively expanded or reduced scale, to avoid the
     possibility of numerical problems inside `kdu_region_decompressor' -- the
     bounds are, in practice, far beyond anything you are likely to
     encounter in the real world. */
{
  if (scale > (float) max_safe_scale_x)
    scale = (float) max_safe_scale_x;
  if (scale > (float) max_safe_scale_y)
    scale = (float) max_safe_scale_y;
  if (scale*scale < (float) min_safe_scale_prod)
    scale = (float) sqrt(min_safe_scale_prod);

  int num, den=1;
  if (scale > (float)(1<<30))
    scale = (float)(1<<30); // Avoid ridiculous factors
  bool close_enough = true;
  do {
      if (!close_enough)
        den <<= 1;
      num = (int)(scale * den + 0.5F);
      float ratio = num / (scale * den);
      close_enough = (ratio > 0.9999F) && (ratio < 1.0001F);
    } while ((!close_enough) && (den < (1<<28)) && (num < (1<<28)));
  numerator.x = numerator.y = num;
  denominator.x = denominator.y = den;
}

//----------------------------------------------------------------------------
int vtkKakaduReader::RequestInformation(
    vtkInformation* request,
    vtkInformationVector** inputVector,
    vtkInformationVector* outputVector)
{
  if(this->FileName == NULL)
    {
    vtkErrorMacro("FileName not set.");
    return 0;
    }

  int retval;
  retval =
    this->Superclass::RequestInformation(request, inputVector, outputVector);
  if (retval != VTK_OK)
    {
    return retval;
    }

  vtkInformation* outInfo = outputVector->GetInformationObject(0);
  outInfo->Set(vtkDataObject::ORIGIN(),this->Origin,3);
  outInfo->Set(vtkDataObject::SPACING(), this->Spacing, 3);

  //GET WHOLE EXTENT FROM FILE
  //TODO: cache result and only do this when filename changes
  jp2_family_src src;
  jp2_input_box box;
  src.open(this->FileName);
  bool is_jp2 = box.open(&src) && (box.get_box_type() == jp2_signature_4cc);
  src.close();
  kdu_compressed_source *input = NULL;
  kdu_simple_file_source file_in;
  jp2_family_src jp2_ultimate_src;
  jp2_source jp2_in;
  if (!is_jp2)
    {
    file_in.open(this->FileName);
    input = &file_in;
    }
  else
    {
    jp2_ultimate_src.open(this->FileName);
    if (!jp2_in.open(&jp2_ultimate_src))
      { kdu_error e; e << "Supplied input file, \"" << this->FileName
                       << "\", does not appear to contain any boxes."; }
    if (!jp2_in.read_header())
      assert(0);
    input = &jp2_in;
    }
  kdu_codestream codestream;
  codestream.create(input);
  kdu_dims dims;
  codestream.get_dims(0, dims);
  kdu_coords *coords;
  coords = dims.access_size();
  int width = coords->get_x();
  int height = coords->get_y();
  int depth = codestream.get_num_components();
  /*
  cerr << "WIDTH " << width
       << " HEIGHT " << height
       << " DEPTH " << depth << endl;
  */
  this->Internals->WholeExtent[0] = 0;
  this->Internals->WholeExtent[1] = width-1;
  this->Internals->WholeExtent[2] = 0;
  this->Internals->WholeExtent[3] = height-1;
  this->Internals->WholeExtent[4] = 0;
  this->Internals->WholeExtent[5] = depth-1;
  this->Internals->ResLevels = codestream.get_min_dwt_levels();
  //cerr << "DWT LEVELS " << this->Internals->ResLevels << endl;

  //TODO: close file?

  int sWholeExtent[6];
  sWholeExtent[0] = this->Internals->WholeExtent[0];
  sWholeExtent[1] = this->Internals->WholeExtent[1];
  sWholeExtent[2] = this->Internals->WholeExtent[2];
  sWholeExtent[3] = this->Internals->WholeExtent[3];
  sWholeExtent[4] = this->Internals->WholeExtent[4];
  sWholeExtent[5] = this->Internals->WholeExtent[5];
  outInfo->Set
    (vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), sWholeExtent, 6);

  double sSpacing[3];
  sSpacing[0] = this->Spacing[0];
  sSpacing[1] = this->Spacing[1];
  sSpacing[2] = this->Spacing[2];
/*
  cerr << "Spacing : " 
       << sSpacing[0] << "," << sSpacing[1] << "," << sSpacing[1] 
       << endl;
  cerr << "WholeExtent : "
       << sWholeExtent[0] << " " << sWholeExtent[1] << ","
       << sWholeExtent[2] << " " << sWholeExtent[3] << ","
       << sWholeExtent[4] << " " << sWholeExtent[5] << endl;
*/
  double rRes = 0.0;
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_RESOLUTION()))
    {
    rRes =
      outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_RESOLUTION());
    }
  else
    {
    rRes = 0.0;
//    cerr << "NO RES REQUESTED" << endl;
    }
  int level = (int)((1.0-rRes)*(double)this->Internals->ResLevels);
  //cerr << "RES " << rRes << " -> LEVEL " << level << endl;

#if 1
  float scale = 1.0;
  int discard_levs=level;
  int max_discard_levs = this->Internals->ResLevels;
  //while ((scale < 0.6F) && (discard_levs < max_discard_levs))
  //  { scale *= 2.0F; discard_levs++; }
  double min_scale_prod, max_scale_x, max_scale_y;
  kdu_region_decompressor decompressor;
  
  //ask for the Z plane we are now working on
  codestream.apply_input_restrictions(0,1,
                                      0,0,
                                      NULL,
                                      KDU_WANT_CODESTREAM_COMPONENTS);
      
  decompressor.get_safe_expansion_factors
    (codestream,
     NULL, 0,
     discard_levs,
     min_scale_prod,
     max_scale_x,max_scale_y,
     KDU_WANT_CODESTREAM_COMPONENTS);

  kdu_coords scale_num, scale_den;
  find_rational_scale(scale,scale_num,scale_den,
                      min_scale_prod,max_scale_x,max_scale_y);
  
  //find the whole res for this level
  kdu_dims image_dims, image_dims_all;
  image_dims_all = decompressor.get_rendered_image_dims
    (codestream,
     NULL, 0, //ask for just one Zplane
     discard_levs, //ask for the specified resolution level
     scale_num, scale_den,
     KDU_WANT_CODESTREAM_COMPONENTS);

//  cerr << "WHOLE PLANE " << image_dims_all.area() << endl;
  sWholeExtent[1] = image_dims_all.size.x-1;
  sWholeExtent[3] = image_dims_all.size.y-1;
  for (int i = level; i > 0; i--)
    {
    sSpacing[0] = sSpacing[0] * 2;
    sSpacing[1] = sSpacing[1] * 2;
    }
#else
  for (int i = level; i > 0; i--)
    {
    //TODO: if ceiling is not always right, find way to ask kakadu
    //for the dims of each level directly.
    sSpacing[0] = sSpacing[0] * 2;
    sSpacing[1] = sSpacing[1] * 2;
    //sSpacing[2] = sSpacing[2] * 2;
    int dx = sWholeExtent[1] - sWholeExtent[0];
    if ((float)(dx/2) != ((float)dx)/2.0)
      {
      dx = dx + 1;
      }
    sWholeExtent[1] = sWholeExtent[0] + dx/2;

    int dy = sWholeExtent[3] - sWholeExtent[2];
    if ((float)(dy/2) != ((float)dy)/2.0)
      {
      dy = dy + 1;
      }
    sWholeExtent[3] = sWholeExtent[2] + dy/2;

    int dz = sWholeExtent[5] - sWholeExtent[4];
    //if ((float)(dz/2) != ((float)dz)/2.0)
    //  {
    //  dz = dz + 1;
    //  }
    //sWholeExtent[5] = sWholeExtent[4] + dz/2;

    //cerr << "LEVEL " << i << endl;
    }
#endif

/*
  cerr << "sSpacing : " 
       << sSpacing[0] << "," << sSpacing[1] << "," << sSpacing[2] 
       << endl;
  cerr << "sWholeExtent : "
       << sWholeExtent[0] << " " << sWholeExtent[1] << ","
       << sWholeExtent[2] << " " << sWholeExtent[3] << ","
       << sWholeExtent[4] << " " << sWholeExtent[5] << endl;
*/
  outInfo->Set
    (vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(), sWholeExtent, 6);
  outInfo->Set(vtkDataObject::SPACING(), sSpacing, 3);

  this->Internals->Resolution = rRes;

  outInfo->Get(vtkDataObject::ORIGIN(), this->Origin);
  double bounds[6];
  bounds[0] = this->Origin[0] + sSpacing[0] * sWholeExtent[0];
  bounds[1] = this->Origin[0] + sSpacing[0] * sWholeExtent[1];
  bounds[2] = this->Origin[1] + sSpacing[1] * sWholeExtent[2];
  bounds[3] = this->Origin[1] + sSpacing[1] * sWholeExtent[3];
  bounds[4] = this->Origin[2] + sSpacing[2] * sWholeExtent[4];
  bounds[5] = this->Origin[2] + sSpacing[2] * sWholeExtent[5];
  DEBUGPRINT_RESOLUTION
    (
     cerr << "RI SET BOUNDS ";
     {for (int i = 0; i < 6; i++) cerr << bounds[i] << " ";}
     cerr << endl;
     );
  outInfo->Set(vtkStreamingDemandDrivenPipeline::WHOLE_BOUNDING_BOX(),
       bounds, 6);

  return 1;
}

//----------------------------------------------------------------------------
// Setting extents of the rectilinear grid
int vtkKakaduReader::RequestData(vtkInformation* request,
    vtkInformationVector** vtkNotUsed(inputVector),
    vtkInformationVector* outputVector  )
{
//  cerr << "REQUEST DATA" << endl;
  this->UpdateProgress(0);

  // get the data object
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  vtkDataObject* output = outInfo->Get(vtkDataObject::DATA_OBJECT());
  vtkImageData *imageData = vtkImageData::SafeDownCast(output);
  //imageData->Initialize();

  //get parameters of what the pipeline is looking for
  double sSpacing[3];
  outInfo->Get(vtkDataObject::SPACING(), sSpacing);
  double range[2];
  int P = outInfo->Get
    (vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
  int NP = outInfo->Get
    (vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
  double rRes = 0.0;
  if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_RESOLUTION()))
    {
    rRes =
      outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_RESOLUTION());
    }
  else
    {
    rRes = 0.0;
//    cerr << "NO RES REQUESTED" << endl;
    }

  int wext[6];
  outInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(),wext);
  int subext[6];
  outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),subext);

  int level = (int)((1.0-rRes)*(double)this->Internals->ResLevels);

  //cerr << "REQUESTED SUBEXTENT IS "
  //     << P << "/" << NP << "@" << rRes << "->" << level << " "
  //     << subext[0] << "," << subext[1] << ","
  //     << subext[2] << "," << subext[3] << ","
  //     << subext[4] << "," << subext[5] << endl;

  if (subext[1] < subext[0] || 
      subext[3] < subext[2] || 
      subext[5] < subext[4])
    {
    cerr << "NO SUBEXTENT REQUESTED" << endl;
    return 1;
    }
  if (rRes != 0)
    {
    if ((subext[0] == wext[0]) && 
        (subext[1] == wext[1]) && 
        (subext[2] == wext[2]) && 
        (subext[3] == wext[3]) && 
        (subext[4] == wext[4]) && 
        (subext[5] == wext[5]))
      {
      cerr << "ASKED FOR WHOLE EXTENT" << endl;
      return 1;
      }
    }
  imageData->SetExtent(subext);

//  cerr << "RES " << rRes << " -> LEVEL " << level << endl;
  vtkInformation *dInfo = imageData->GetInformation();
  dInfo->Set(vtkDataObject::DATA_RESOLUTION(), rRes);

  vtkIdType w_numberOfTuples;
  int dx = subext[1]-subext[0]+1;
  if (dx < 1) dx = 1;
  int dy = subext[3]-subext[2]+1;
  if (dy < 1) dy = 1;
  int dz = subext[5]-subext[4]+1;
  if (dz < 1) dz = 1;
  w_numberOfTuples = dx*dy*dz;
/*
  cerr << "WHOLETUPLES " 
       << dx << "x" << dy << "x" << dz << "=" << w_numberOfTuples << endl;
*/
  int depth = this->Internals->WholeExtent[5]-this->Internals->WholeExtent[4]+1;
  if (depth < 1) depth = 1;

  //read requested region at requested resolution into buffer
  unsigned int *buffer;
  buffer = new unsigned int[w_numberOfTuples];
  float scale = 1.0;
  jp2_family_src src;
  jp2_input_box box;
  src.open(this->FileName);
  bool is_jp2 = box.open(&src) && (box.get_box_type() == jp2_signature_4cc);
  src.close();
  kdu_dims image_dims, image_dims_all;
  kdu_dims new_region, incomplete_region;
  kdu_uint32 *planebuffer = NULL;

  try { 
    do { // Loop around in the odd event that the number of discard
    // levels is found to be too large after processing starts.
    if (planebuffer != NULL)
      {
      kdu_warning w; 
      w << "Discovered a tile with fewer DWT levels part "
        "way through processing; repeating the rendering process with "
        "fewer DWT levels, together with resolution reduction signal "
        "processing.";
      delete[] planebuffer; planebuffer = NULL;
      }
    
    for (int zPlane = 0; zPlane < depth; zPlane++)
      {
      //TODO: Ask kakadu for more than one zplane 
      //at a time, or at least to avoid closing and reopening the file 
      //to advance to the next one.
//      cerr << "ZPLANE " << zPlane << endl;
      
      // Open a suitable compressed data source object
      kdu_compressed_source *input = NULL;
      kdu_simple_file_source file_in;
      jp2_family_src jp2_ultimate_src;
      jp2_source jp2_in;
      if (!is_jp2)
        {
        file_in.open(this->FileName);
        input = &file_in;
        }
      else
        {
        jp2_ultimate_src.open(this->FileName);
        if (!jp2_in.open(&jp2_ultimate_src))
          { kdu_error e; e << "Supplied input file, \"" << this->FileName
                           << "\", does not appear to contain any boxes."; }
        if (!jp2_in.read_header())
          assert(0); // Should not be possible for file-based sources
        input = &jp2_in;
        }
      
      // Create the code-stream management machinery for the compressed data
      kdu_codestream codestream;
      codestream.create(input);
      
      // Configure a `kdu_channel_mapping' object for `kdu_region_decompressor'
      kdu_channel_mapping channels;
      if (jp2_in.exists())
        {
        channels.configure(&jp2_in,false);
        }
      else
        {
        channels.configure(codestream);
        }
      
      int discard_levs=level;
      int max_discard_levs = this->Internals->ResLevels;
      //while ((scale < 0.6F) && (discard_levs < max_discard_levs))
      //  { scale *= 2.0F; discard_levs++; }
      double min_scale_prod, max_scale_x, max_scale_y;
      kdu_region_decompressor decompressor;

      //ask for the Z plane we are now working on
      codestream.apply_input_restrictions(zPlane,zPlane+1,
                                          0,0,
                                          NULL,
                                          KDU_WANT_CODESTREAM_COMPONENTS);
      
      decompressor.get_safe_expansion_factors
        (codestream,
         NULL, zPlane,
         discard_levs,
         min_scale_prod,
         max_scale_x,max_scale_y,
         KDU_WANT_CODESTREAM_COMPONENTS);

      kdu_coords scale_num, scale_den;
      find_rational_scale(scale,scale_num,scale_den,
                          min_scale_prod,max_scale_x,max_scale_y);

      //find the whole res for this level
      image_dims_all = decompressor.get_rendered_image_dims
        (codestream,
         NULL, zPlane, //ask for just one Zplane
         discard_levs, //ask for the specified resolution level
         scale_num, scale_den,
         KDU_WANT_CODESTREAM_COMPONENTS);
//      cerr << "WHOLE PLANE " << image_dims_all.area() << endl;

      //specify the portion we want
      image_dims.size.x = dx;
      image_dims.size.y = dy;
      image_dims.pos.x = subext[0];
      image_dims.pos.y = subext[2];
      planebuffer = new kdu_uint32[(size_t) image_dims.area()];

      vtkIdType p_numberOfTuples = image_dims.area();
//      cerr << "PLANETUPLES " << p_numberOfTuples << endl;
      
      decompressor.start
        (codestream,
         NULL,zPlane, //ask for just one Zplane
         discard_levs, INT_MAX, //ask for the specified res level
         image_dims,
         scale_num, scale_den, //expansion numerator/denominator
         true, //precise
         KDU_WANT_CODESTREAM_COMPONENTS, //ask for one Zplane
         false, //slow
         NULL, NULL//threading
        );
      
      incomplete_region=image_dims;
      int i = 0;
      while (decompressor.process((kdu_int32 *)planebuffer, 
                                  image_dims.pos,
                                  image_dims.size.x,
                                  256000,0,
                                  incomplete_region,
                                  new_region))
        {
        // Loop until buffer is filled
        //cerr << "LOOP" << i++ << endl;
        }
      kdu_exception exc=KDU_NULL_EXCEPTION;
      if (!decompressor.finish(&exc))
        {
        cerr << "ERROR" << endl;
        kdu_rethrow(exc);
        }
      memcpy(buffer+(zPlane*p_numberOfTuples),
             planebuffer,
             p_numberOfTuples*sizeof(unsigned int));
      codestream.destroy();
      }
    } while (!incomplete_region.is_empty());
    
    imageData->SetSpacing(sSpacing[0], sSpacing[1], sSpacing[2]);
    unsigned int *ptr = buffer;
    for (int i = 0; i < w_numberOfTuples; i++)
      {
      *ptr = (unsigned int)(((double)(*ptr) - 4285950000.0)/100000.0);
      *ptr++;
      }
    vtkUnsignedIntArray *scalars = vtkUnsignedIntArray::New();
    scalars->SetName("FOO");
    scalars->SetNumberOfComponents(1);
    scalars->SetNumberOfTuples(w_numberOfTuples);
    scalars->SetArray(buffer, w_numberOfTuples, 0, 1);
    scalars->GetRange(range);
    imageData->GetPointData()->SetScalars(scalars);
//    cerr << "RANGE " << range[0] << "," << range[1] << endl;
    this->Internals->RangeKeeper->Insert(P, NP, subext,
                               this->Internals->Resolution,
                               0, "FOO", 0,
                               range);

    scalars->Delete();
  }
  catch (...)
  {
    if (planebuffer != NULL) delete[] planebuffer;
    //codestream.destroy();
    throw; // Rethrow the exception
  }
  if (planebuffer != NULL) delete[] planebuffer;

  return 1;
}

//----------------------------------------------------------------------------
//following 5 functions are used for paraview user interface
void vtkKakaduReader::SelectionModifiedCallback
  (vtkObject*, unsigned long, void* clientdata, void*)
{
  //static_cast<vtkKakaduReader*>(clientdata)->Modified();
}

//-----------------------------------------------------------------------------
int vtkKakaduReader::GetNumberOfVariableArrays()
{
  return this->Internals->VariableArraySelection->GetNumberOfArrays();
}

//-----------------------------------------------------------------------------
const char* vtkKakaduReader::GetVariableArrayName(int index)
{
  if(index < 0 || index >= this->GetNumberOfVariableArrays())
    {
    return NULL;
    }
  return this->Internals->VariableArraySelection->GetArrayName(index);
}

//-----------------------------------------------------------------------------
int vtkKakaduReader::GetVariableArrayStatus(const char* name)
{
  return this->Internals->VariableArraySelection->ArrayIsEnabled(name);
}

//-----------------------------------------------------------------------------
void vtkKakaduReader::SetVariableArrayStatus(const char* name, int status)
{
  vtkDebugMacro("Set cell array \"" << name << "\" status to: " << status);
  if(this->Internals->VariableArraySelection->ArrayExists(name) == 0)
    {
    vtkErrorMacro(<< name << " is not available in the file.");
    return;
    }
  int enabled = this->Internals->VariableArraySelection->ArrayIsEnabled(name);
  if(status != 0 && enabled == 0)
    {
    this->Internals->VariableArraySelection->EnableArray(name);
    //this->Modified();
    }
  else if(status == 0 && enabled != 0)
    {
    this->Internals->VariableArraySelection->DisableArray(name);
    //this->Modified();
    }
}

//----------------------------------------------------------------------------
int vtkKakaduReader::ProcessRequest(vtkInformation *request,
                                  vtkInformationVector **inputVector,
                                  vtkInformationVector *outputVector)
{
  DEBUGPRINT
    (
     vtkInformation *outInfo = outputVector->GetInformationObject(0);
     int P = outInfo->Get
     (vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
     int NP = outInfo->Get
     (vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
     double res = outInfo->Get
     (vtkStreamingDemandDrivenPipeline::UPDATE_RESOLUTION());
     cerr << "SIP(" << this << ") PR " << P << "/" << NP << "@" << res << "->"
     << this->Internals->SI << " "
     << this->Internals->SJ << " "
     << this->Internals->SK << endl;
     int *ext;
     if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT()))
       {
       ext = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT());
       cerr 
         << ext[0] << "," << ext[1] << " "
         << ext[2] << "," << ext[3] << " "
         << ext[4] << "," << ext[5] << endl;
       }
     else
       {
       cerr << "NO EXT" << endl;
       }
     );

  bool found = false;
  if(request->Has
     (vtkStreamingDemandDrivenPipeline::REQUEST_DATA_NOT_GENERATED()))
    {
    found = true;
    DEBUGPRINT
      (
       cerr << "SIP(" << this << ") RDNG ==============================="<<endl;
       );
    }
  if(request->Has
     (vtkStreamingDemandDrivenPipeline::REQUEST_DATA_OBJECT()))
    {
    found = true;
    DEBUGPRINT
      (
       cerr << "SIP(" << this << ") RDO ==============================="<<endl;
       );
    }
  if(request->Has
     (vtkStreamingDemandDrivenPipeline::REQUEST_INFORMATION()))
    {
    found = true;
    DEBUGPRINT
      (
       cerr << "SIP(" << this << ") RI ==============================="<<endl;
       );
    }
  if(request->Has
     (vtkStreamingDemandDrivenPipeline::REQUEST_RESOLUTION_PROPAGATE()))
    {
    found = true;
    DEBUGPRINT
      (
       cerr << "SIP(" << this << ") RRP ==============================="<<endl;
       );
    }
  if(request->Has
     (vtkStreamingDemandDrivenPipeline::REQUEST_REGENERATE_INFORMATION()))
    {
    found = true;
    DEBUGPRINT
      (
       cerr << "SIP(" << this << ") RRI ==============================="<<endl;
       );
    }
  if(request->Has
     (vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT()))
    {
    found = true;
    DEBUGPRINT
      (
       cerr << "SIP(" << this << ") RUE ==============================="<<endl;
       );
    }
  if(request->Has
     (vtkStreamingDemandDrivenPipeline::REQUEST_UPDATE_EXTENT_INFORMATION()))
    {
    found = true;
    DEBUGPRINT
      (
       cerr << "SIP(" << this << ") RUEI ==============================="<<endl;
       );
    //create meta information for this piece
    double *origin;
    double *spacing;
    int *ext;
    vtkInformation* outInfo = outputVector->GetInformationObject(0);
    origin = outInfo->Get(vtkDataObject::ORIGIN());
    spacing = outInfo->Get(vtkDataObject::SPACING());
    ext = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT());
    int P = outInfo->Get(
      vtkStreamingDemandDrivenPipeline::UPDATE_PIECE_NUMBER());
    int NP = outInfo->Get(
      vtkStreamingDemandDrivenPipeline::UPDATE_NUMBER_OF_PIECES());
    double bounds[6];
    bounds[0] = origin[0] + spacing[0] * ext[0];
    bounds[1] = origin[0] + spacing[0] * ext[1];
    bounds[2] = origin[1] + spacing[1] * ext[2];
    bounds[3] = origin[1] + spacing[1] * ext[3];
    bounds[4] = origin[2] + spacing[2] * ext[4];
    bounds[5] = origin[2] + spacing[2] * ext[5];
    outInfo->Set
      (vtkStreamingDemandDrivenPipeline::PIECE_BOUNDING_BOX(), bounds, 6);

    double range[2];
    vtkInformationVector *miv =
      outInfo->Get(vtkDataObject::POINT_DATA_VECTOR());
    int cnt = 0;
    for(size_t i=0;i<this->Internals->VariableMap.size();i++)
      {
      if(this->Internals->VariableMap[i] != -1 &&
       this->Internals->VariableArraySelection->GetArraySetting
       (this->Internals->VariableMap[i]) != 0)
      {
      const char *name = this->Internals->VariableArraySelection->GetArrayName
        (this->Internals->VariableMap[i]);
      vtkInformation *fInfo = miv->GetInformationObject(cnt);
      if (!fInfo)
        {
        fInfo = vtkInformation::New();
        miv->SetInformationObject(cnt, fInfo);
        fInfo->Delete();
        }
      cnt++;
      range[0] = 0;
      range[1] = -1;
      if (this->Internals->RangeKeeper->Search(P, NP, ext,
                                     0, name, 0,
                                     range))
        {
        DEBUGPRINT_METAINFORMATION
          (
           cerr << "Found range for " << name << " "
           << P << "/" << NP << " "
           << ext[0] << "," << ext[1] << ","
           << ext[2] << "," << ext[3] << ","
           << ext[4] << "," << ext[5] << " is "
           << range[0] << " .. " << range[1] << endl;
           );
        fInfo->Set(vtkDataObject::FIELD_ARRAY_NAME(), name);
        fInfo->Set(vtkDataObject::PIECE_FIELD_RANGE(), range, 2);
        }
      else
        {
        DEBUGPRINT_METAINFORMATION
          (
           cerr << "No range for "
           << ext[0] << "," << ext[1] << ","
           << ext[2] << "," << ext[3] << ","
           << ext[4] << "," << ext[5] << " yet" << endl;
           );
        fInfo->Remove(vtkDataObject::FIELD_ARRAY_NAME());
        fInfo->Remove(vtkDataObject::PIECE_FIELD_RANGE());
        }
      }
      }
    }

  //This is overridden just to intercept requests for debugging purposes.
  if(request->Has(vtkDemandDrivenPipeline::REQUEST_DATA()))
    {
    found = true;
    DEBUGPRINT
      (cerr << "SIP(" << this << ") RD ===============================" << endl;);

    vtkInformation* outInfo = outputVector->GetInformationObject(0);
    int updateExtent[6];
    int wholeExtent[6];
    outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_EXTENT(),
      updateExtent);
    outInfo->Get(vtkStreamingDemandDrivenPipeline::WHOLE_EXTENT(),
      wholeExtent);
    double res = this->Internals->Resolution;
    if (outInfo->Has(vtkStreamingDemandDrivenPipeline::UPDATE_RESOLUTION()))
      {
      res = outInfo->Get(vtkStreamingDemandDrivenPipeline::UPDATE_RESOLUTION());
      }
    bool match = true;
    for (int i = 0; i< 6; i++)
      {
      if (updateExtent[i] != wholeExtent[i])
      {
      match = false;
      }
      }
    if (match && (res == 1.0))
      {
      //vtkErrorMacro("Whole extent requested, streaming is not working.");
      }
    }
  if (!found)
    {
    cerr << "huhn?" << endl;
    request->PrintSelf(cerr, vtkIndent(0));
    }
  int rc = this->Superclass::ProcessRequest(request, inputVector, outputVector);
  return rc;
}
