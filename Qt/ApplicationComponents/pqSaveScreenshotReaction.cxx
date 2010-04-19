/*=========================================================================

   Program: ParaView
   Module:    pqSaveScreenshotReaction.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.2. 

   See License_v1.2.txt for the full ParaView license.
   A copy of this license can be obtained by contacting
   Kitware Inc.
   28 Corporate Drive
   Clifton Park, NY 12065
   USA

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHORS OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

========================================================================*/
#include "pqSaveScreenshotReaction.h"

#include "pqActiveObjects.h"
#include "pqCoreUtilities.h"
#include "pqFileDialog.h"
#include "pqImageUtil.h"
#include "pqPVApplicationCore.h"
#include "pqRenderViewBase.h"
#include "pqSaveSnapshotDialog.h"
#include "pqSettings.h"
#include "pqView.h"
#include "pqViewManager.h"
#include "vtkImageData.h"
#include "vtkPVXMLElement.h"
#include "vtkSmartPointer.h"

#include "vtkPVConfig.h"
#ifdef PARAVIEW_ENABLE_PYTHON
#include "pqPythonManager.h"
#include "pqPythonDialog.h"
#include "pqPythonShell.h"
#endif

#include <QDebug>
#include <QFileInfo>

#ifdef VTK_USE_GL2PS
#include "vtkGL2PSExporter.h"
#include "vtkRenderWindow.h"
#include "pqRenderView.h"
#include "QVTKWidget.h"
#endif

//-----------------------------------------------------------------------------
pqSaveScreenshotReaction::pqSaveScreenshotReaction(QAction* parentObject)
  : Superclass(parentObject)
{
  // load state enable state depends on whether we are connected to an active
  // server or not and whether
  pqActiveObjects* activeObjects = &pqActiveObjects::instance();
  QObject::connect(activeObjects, SIGNAL(serverChanged(pqServer*)),
    this, SLOT(updateEnableState()));
  QObject::connect(activeObjects, SIGNAL(viewChanged(pqView*)),
    this, SLOT(updateEnableState()));
  this->updateEnableState();
}

//-----------------------------------------------------------------------------
void pqSaveScreenshotReaction::updateEnableState()
{
  pqActiveObjects* activeObjects = &pqActiveObjects::instance();
  bool is_enabled = (activeObjects->activeView() && activeObjects->activeServer());
  this->parentAction()->setEnabled(is_enabled);
}

//-----------------------------------------------------------------------------
void pqSaveScreenshotReaction::saveScreenshot()
{
  pqViewManager* viewManager = qobject_cast<pqViewManager*>(
    pqApplicationCore::instance()->manager("MULTIVIEW_MANAGER"));
  if (!viewManager)
    {
    qCritical("Could not locate pqViewManager. If using custom-widget as the "
      "central widget, you cannot use pqSaveScreenshotReaction.");
    return;
    }

  pqView* view = pqActiveObjects::instance().activeView();
  if (!view)
    {
    qDebug() << "Cannnot save image. No active view.";
    return;
    }

  pqSaveSnapshotDialog ssDialog(pqCoreUtilities::mainWidget());
  ssDialog.setViewSize(view->getSize());
  ssDialog.setAllViewsSize(viewManager->clientSize());

  if (ssDialog.exec() != QDialog::Accepted)
    {
    return;
    }

  QString lastUsedExt;
  // Load the most recently used file extensions from QSettings, if available.
  pqSettings* settings = pqApplicationCore::instance()->settings();
  if (settings->contains("extensions/ScreenshotExtension"))
    {
    lastUsedExt = 
      settings->value("extensions/ScreenshotExtension").toString();
    }

  QString filters;
  filters += "PNG image (*.png)";
  filters += ";;BMP image (*.bmp)";
  filters += ";;TIFF image (*.tif)";
  filters += ";;PPM image (*.ppm)";
  filters += ";;JPG image (*.jpg)";
  filters += ";;PDF file (*.pdf)";
#ifdef VTK_USE_GL2PS
  const QString type (view->getViewType());
  // Reason for this limitation to RenderView is that class vtkExporter
  // only supports RenderView objects.
  if (type == pqRenderView::renderViewType())
    {
    filters += ";;Postscript file (*.ps)";
    filters += ";;Encapsulated Postscript file (*.eps)";
    filters += ";;Scalable Vector Graphics file (*.svg)";
    }
#endif
  pqFileDialog file_dialog(NULL,
    pqCoreUtilities::mainWidget(),
    tr("Save Screenshot:"), QString(), filters);
  file_dialog.setRecentlyUsedExtension(lastUsedExt);
  file_dialog.setObjectName("FileSaveScreenshotDialog");
  file_dialog.setFileMode(pqFileDialog::AnyFile);
  if (file_dialog.exec() != QDialog::Accepted)
    {
    return;
    }

  QString file = file_dialog.getSelectedFiles()[0];
  QFileInfo fileInfo = QFileInfo( file );
  lastUsedExt = QString("*.") + fileInfo.suffix();
  settings->setValue("extensions/ScreenshotExtension", lastUsedExt);

  QSize size = ssDialog.viewSize();
  QString palette = ssDialog.palette();

  // temporarily load the color palette chosen by the user.
  vtkSmartPointer<vtkPVXMLElement> currentPalette;
  pqApplicationCore* core = pqApplicationCore::instance();
  if (!palette.isEmpty())
    {
    currentPalette.TakeReference(core->getCurrrentPalette());
    core->loadPalette(palette);
    }

  int stereo = ssDialog.getStereoMode();
  if (stereo)
    {
    pqRenderViewBase::setStereo(stereo);
    }

  pqSaveScreenshotReaction::saveScreenshot(file,
    size, ssDialog.quality(), ssDialog.saveAllViews());

  // restore palette.
  if (!palette.isEmpty())
    {
    core->loadPalette(currentPalette);
    }

  if (stereo)
    {
    pqRenderViewBase::setStereo(0);
    core->render();
    }
}

//-----------------------------------------------------------------------------
void pqSaveScreenshotReaction::saveScreenshot(
  const QString& filename, const QSize& size, int quality, bool all_views)
{
  pqViewManager* viewManager = qobject_cast<pqViewManager*>(
    pqApplicationCore::instance()->manager("MULTIVIEW_MANAGER"));
  if (!viewManager)
    {
    qCritical("Could not locate pqViewManager. If using custom-widget as the "
      "central widget, you cannot use pqSaveScreenshotReaction.");
    return;
    }
  pqView* view = pqActiveObjects::instance().activeView();
  vtkSmartPointer<vtkImageData> img;
  if (all_views)
    {
    img.TakeReference(
      viewManager->captureImage(size.width(), size.height()));
    }
  else if (view)
    {
    img.TakeReference(view->captureImage(size));
    }

  if (img.GetPointer() == NULL)
    {
    qCritical() << "Save Image failed.";
    }
  else
    {
#ifdef VTK_USE_GL2PS
    // (Code does not respect magnification requests, but that does not make
    //  a difference as increasing magnification does not improve
    //  rendering resolution in ParaView currently.)

    QFileInfo fileInfo = QFileInfo( filename );
    if (fileInfo.suffix() == "ps" || fileInfo.suffix() == "eps" || fileInfo.suffix() == "svg")
      {
      QString filePsExporter (filename);
      const QString type (view->getViewType());

      // Reason for this limitation to RenderView is that class vtkExporter - the superclass
      // of vtkGL2PSExporter and the underlying library GL2PS - only supports RenderView objects.
      if (type != pqRenderView::renderViewType())
        {
        qCritical() << "Vector graphics output only supported for RenderView objects (= 2D/3D views).";
        return;
        }

      pqRenderView* const render_module = qobject_cast<pqRenderView*>(view);
      if(!render_module)
        {
        qCritical() << "Cannnot save image. No active render module.";
        return;
        }

      QVTKWidget* const widget =
        qobject_cast<QVTKWidget*>(render_module->getWidget());
      if (!widget)
        {
        qCritical() << "Cannnot save image. No widget available.";
        return;
        }

      vtkSmartPointer< vtkGL2PSExporter > psExporter( vtkGL2PSExporter::New() );

      // Pass current view to GL2PS
      // Simple variant
      psExporter->SetRenderWindow(widget->GetRenderWindow());

      // More complex variant:
      // enforce maximal smoothing. Not sure whether this makes a difference or
      // is needed at all, though.
//    vtkRenderWindow *smoothing = vtkRenderWindow::New();
//    smoothing = widget->GetRenderWindow();
//    smoothing->LineSmoothingOn();
//    smoothing->PointSmoothingOn();
//    smoothing->PolygonSmoothingOn();
//    psExporter->SetRenderWindow(smoothing);

      // Deny generation of mixed vector/raster images.
      // (All the 3D props in the scene would be written as a raster image
      //  and all 2D actors will be written as vector graphic primitives.
      //  This makes it possible to handle transparency and complex 3D scenes.)
      psExporter->Write3DPropsAsRasterImageOff();

      // Treat filename, it already contains a suffix which needs to be cut off
      // because psExporter->Write will automatically append the suitable suffix.
      if( filePsExporter.toLower().endsWith( ".ps" ) )
        {
        filePsExporter.chop( 3 );
        psExporter->SetFileFormatToPS();
        }
      if( filePsExporter.toLower().endsWith( ".eps" ) )
        {
        filePsExporter.chop( 4 );
        psExporter->SetFileFormatToEPS();
        }
      if( filePsExporter.toLower().endsWith( ".svg" ) )
        {
        filePsExporter.chop( 4 );
        psExporter->SetFileFormatToSVG();
        }
      psExporter->SetFilePrefix( filePsExporter.toStdString().c_str() );

      psExporter->CompressOff();
      psExporter->DrawBackgroundOn();
      psExporter->TextOn();
      psExporter->PS3ShadingOn();
      psExporter->OcclusionCullOn();
      psExporter->SetSortToBSP();

      psExporter->Write();
      psExporter->Delete();
      } // end suffix = ps|eps|svg
    else
      {
#endif
    pqImageUtil::saveImage(img, filename, quality);
#ifdef VTK_USE_GL2PS
      }
#endif
    }

#ifdef PARAVIEW_ENABLE_PYTHON
  pqPythonManager* manager = pqPVApplicationCore::instance()->pythonManager();
  if (manager && manager->interpreterIsInitialized())
    {
    QString allViewsStr = all_views ? "True" : "False";
    QString script =
    "try:\n"
    "  paraview.smtrace\n"
    "  paraview.smtrace.trace_save_screenshot('%1', (%2, %3), %4)\n"
    "except AttributeError: pass\n";
    script = script.arg(filename).arg(size.width()).arg(size.height()).arg(allViewsStr);
    pqPythonShell* shell = manager->pythonShellDialog()->shell();
    shell->executeScript(script);
    return;
    }
#endif
}
