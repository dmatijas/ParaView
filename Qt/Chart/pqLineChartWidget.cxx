/*=========================================================================

   Program: ParaView
   Module:    pqLineChartWidget.cxx

   Copyright (c) 2005,2006 Sandia Corporation, Kitware Inc.
   All rights reserved.

   ParaView is a free software; you can redistribute it and/or modify it
   under the terms of the ParaView license version 1.1. 

   See License_v1.1.txt for the full ParaView license.
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

=========================================================================*/

/// \file pqLineChartWidget.cxx
/// \date 9/27/2005

#include "pqLineChartWidget.h"

#include "pqChartArea.h"
#include "pqChartAxis.h"
#include "pqChartInteractor.h"
#include "pqChartWidget.h"
#include "pqLineChart.h"
#include <QWidget>


pqChartWidget *pqLineChartWidget::createLineChart(QWidget *parent,
    pqLineChart **layer)
{
  pqChartWidget *chart = new pqChartWidget(parent);

  // Get the chart area and set up the axes.
  pqChartArea *chartArea = chart->getChartArea();
  chartArea->createAxis(pqChartAxis::Left);
  chartArea->createAxis(pqChartAxis::Bottom);

  // Add the default chart interactor.
  chartArea->setInteractor(new pqChartInteractor(chartArea));

  // Set up the line chart.
  pqLineChart *lineChart = new pqLineChart(chartArea);
  lineChart->setAxes(chartArea->getAxis(pqChartAxis::Bottom),
      chartArea->getAxis(pqChartAxis::Left));
  chartArea->addLayer(lineChart);

  // Pass back a pointer to the line chart layer.
  if(layer)
    {
    *layer = lineChart;
    }

  return chart;
}


