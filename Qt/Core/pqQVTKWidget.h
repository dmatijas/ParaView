/*=========================================================================

   Program: ParaView
   Module:    $RCSfile$

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
#ifndef __pqQVTKWidget_h
#define __pqQVTKWidget_h

#include "QVTKWidget.h"
#include "pqCoreExport.h"
#include "vtkSmartPointer.h"
#include "vtkSynchronizedRenderers.h" // needed for vtkRawImage;
#include <QPointer>

class vtkSMProxy;
class QTimer;

/// pqQVTKWidget extends QVTKWidget to add awareness for view proxies. The
/// advantage of doing that is that pqQVTKWidget can automatically update the
/// "ViewPosition" and "ViewSize" properties on the view proxy whenever the
/// widget's size/position changes.
///
/// pqQVTKWidget also enables image caching by default. Additionally, it does
/// some extra tricks to optimize the use of the cache when doing transient
/// actions such as resizing.
class PQCORE_EXPORT pqQVTKWidget : public QVTKWidget
{
  Q_OBJECT
  typedef QVTKWidget Superclass;

  Q_PROPERTY(QWidget* positionReference
    READ positionReference
    WRITE setPositionReference)

public:
  pqQVTKWidget(QWidget* parent = NULL, Qt::WFlags f = 0);
  virtual ~pqQVTKWidget();

  /// Set the view proxy.
  void setViewProxy(vtkSMProxy*);

  /// If none is specified, then the parentWidget() is used. Position reference
  /// is a widget (typically an ancestor of this pqQVTKWidget) relative to which
  /// the position for the pqQVTKWidget should be determined.
  void setPositionReference(QWidget* widget);

  QWidget* positionReference() const;

protected:
  /// overloaded resize handler
  virtual void resizeEvent(QResizeEvent* event);

  /// overloaded move handler
  virtual void moveEvent(QMoveEvent* event);

  // method called in paintEvent() to render the image cache on to the device.
  // return false, if cache couldn;t be used for painting. In that case, the
  // paintEvent() method will continue with the default painting code.
  virtual bool paintCachedImage();
private slots:
  void updateSizeProperties();

private:
  Q_DISABLE_COPY(pqQVTKWidget)
  vtkSmartPointer<vtkSMProxy> ViewProxy;
  QPointer<QWidget> PositionReference;
  /// used to collapse multiple resize events.
  QTimer* ResizeTimer;
  vtkSynchronizedRenderers::vtkRawImage *ResizingImage;
  bool Resizing;
};

#endif
