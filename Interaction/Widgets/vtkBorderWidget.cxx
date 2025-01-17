/*=========================================================================

  Program:   Visualization Toolkit
  Module:    vtkBorderWidget.cxx

  Copyright (c) Ken Martin, Will Schroeder, Bill Lorensen
  All rights reserved.
  See Copyright.txt or http://www.kitware.com/Copyright.htm for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
#include "vtkBorderWidget.h"
#include "vtkBorderRepresentation.h"
#include "vtkCallbackCommand.h"
#include "vtkCommand.h"
#include "vtkEvent.h"
#include "vtkObjectFactory.h"
#include "vtkRenderWindow.h"
#include "vtkRenderWindowInteractor.h"
#include "vtkRenderer.h"
#include "vtkWidgetCallbackMapper.h"
#include "vtkWidgetEvent.h"
#include "vtkWidgetEventTranslator.h"

vtkStandardNewMacro(vtkBorderWidget);

//------------------------------------------------------------------------------
vtkBorderWidget::vtkBorderWidget()
{
  this->WidgetState = vtkBorderWidget::Start;
  this->Selectable = 1;
  this->Resizable = 1;

  this->CallbackMapper->SetCallbackMethod(
    vtkCommand::LeftButtonPressEvent, vtkWidgetEvent::Select, this, vtkBorderWidget::SelectAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::LeftButtonReleaseEvent,
    vtkWidgetEvent::EndSelect, this, vtkBorderWidget::EndSelectAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::MiddleButtonPressEvent,
    vtkWidgetEvent::Translate, this, vtkBorderWidget::TranslateAction);
  this->CallbackMapper->SetCallbackMethod(vtkCommand::MiddleButtonReleaseEvent,
    vtkWidgetEvent::EndSelect, this, vtkBorderWidget::EndSelectAction);
  this->CallbackMapper->SetCallbackMethod(
    vtkCommand::MouseMoveEvent, vtkWidgetEvent::Move, this, vtkBorderWidget::MoveAction);
  this->CallbackMapper->SetCallbackMethod(
    vtkCommand::HoverEvent, vtkWidgetEvent::HoverLeave, this, vtkBorderWidget::HoverLeaveAction);
}

//------------------------------------------------------------------------------
vtkBorderWidget::~vtkBorderWidget() = default;

//------------------------------------------------------------------------------
void vtkBorderWidget::SetCursor(int cState)
{
  if (!this->Resizable && cState != vtkBorderRepresentation::Inside)
  {
    this->RequestCursorShape(VTK_CURSOR_DEFAULT);
    return;
  }

  switch (cState)
  {
    case vtkBorderRepresentation::AdjustingP0:
      this->RequestCursorShape(VTK_CURSOR_SIZESW);
      break;
    case vtkBorderRepresentation::AdjustingP1:
      this->RequestCursorShape(VTK_CURSOR_SIZESE);
      break;
    case vtkBorderRepresentation::AdjustingP2:
      this->RequestCursorShape(VTK_CURSOR_SIZENE);
      break;
    case vtkBorderRepresentation::AdjustingP3:
      this->RequestCursorShape(VTK_CURSOR_SIZENW);
      break;
    case vtkBorderRepresentation::AdjustingE0:
    case vtkBorderRepresentation::AdjustingE2:
      this->RequestCursorShape(VTK_CURSOR_SIZENS);
      break;
    case vtkBorderRepresentation::AdjustingE1:
    case vtkBorderRepresentation::AdjustingE3:
      this->RequestCursorShape(VTK_CURSOR_SIZEWE);
      break;
    case vtkBorderRepresentation::Inside:
      if (reinterpret_cast<vtkBorderRepresentation*>(this->WidgetRep)->GetMoving())
      {
        this->RequestCursorShape(VTK_CURSOR_SIZEALL);
      }
      else
      {
        this->RequestCursorShape(VTK_CURSOR_HAND);
      }
      break;
    default:
      this->RequestCursorShape(VTK_CURSOR_DEFAULT);
  }
}

//------------------------------------------------------------------------------
void vtkBorderWidget::SelectAction(vtkAbstractWidget* w)
{
  vtkBorderWidget* self = reinterpret_cast<vtkBorderWidget*>(w);

  if (self->SubclassSelectAction() ||
    self->WidgetRep->GetInteractionState() == vtkBorderRepresentation::Outside)
  {
    return;
  }

  // We are definitely selected
  self->GrabFocus(self->EventCallbackCommand);
  self->WidgetState = vtkBorderWidget::Selected;

  // Picked something inside the widget
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];

  // This is redundant but necessary on some systems (windows) because the
  // cursor is switched during OS event processing and reverts to the default
  // cursor (i.e., the MoveAction may have set the cursor previously, but this
  // method is necessary to maintain the proper cursor shape)..
  self->SetCursor(self->WidgetRep->GetInteractionState());

  // convert to normalized viewport coordinates
  double XF = static_cast<double>(X);
  double YF = static_cast<double>(Y);
  self->CurrentRenderer->DisplayToNormalizedDisplay(XF, YF);
  self->CurrentRenderer->NormalizedDisplayToViewport(XF, YF);
  self->CurrentRenderer->ViewportToNormalizedViewport(XF, YF);
  double eventPos[2];
  eventPos[0] = XF;
  eventPos[1] = YF;
  self->WidgetRep->StartWidgetInteraction(eventPos);

  if (self->Selectable && self->WidgetRep->GetInteractionState() == vtkBorderRepresentation::Inside)
  {
    vtkBorderRepresentation* rep = reinterpret_cast<vtkBorderRepresentation*>(self->WidgetRep);
    double* fpos1 = rep->GetPositionCoordinate()->GetValue();
    double* fpos2 = rep->GetPosition2Coordinate()->GetValue();

    eventPos[0] = (XF - fpos1[0]) / fpos2[0];
    eventPos[1] = (YF - fpos1[1]) / fpos2[1];

    self->SelectRegion(eventPos);
  }

  self->EventCallbackCommand->SetAbortFlag(1);
  self->StartInteraction();
  self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
}

//------------------------------------------------------------------------------
void vtkBorderWidget::TranslateAction(vtkAbstractWidget* w)
{
  vtkBorderWidget* self = reinterpret_cast<vtkBorderWidget*>(w);

  if (self->SubclassTranslateAction() ||
    self->WidgetRep->GetInteractionState() == vtkBorderRepresentation::Outside)
  {
    return;
  }

  // We are definitely selected
  self->GrabFocus(self->EventCallbackCommand);
  self->WidgetState = vtkBorderWidget::Selected;
  reinterpret_cast<vtkBorderRepresentation*>(self->WidgetRep)->MovingOn();

  // Picked something inside the widget
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];

  // This is redundant but necessary on some systems (windows) because the
  // cursor is switched during OS event processing and reverts to the default
  // cursor.
  self->SetCursor(self->WidgetRep->GetInteractionState());

  // convert to normalized viewport coordinates
  double XF = static_cast<double>(X);
  double YF = static_cast<double>(Y);
  self->CurrentRenderer->DisplayToNormalizedDisplay(XF, YF);
  self->CurrentRenderer->NormalizedDisplayToViewport(XF, YF);
  self->CurrentRenderer->ViewportToNormalizedViewport(XF, YF);
  double eventPos[2];
  eventPos[0] = XF;
  eventPos[1] = YF;
  self->WidgetRep->StartWidgetInteraction(eventPos);

  self->EventCallbackCommand->SetAbortFlag(1);
  self->StartInteraction();
  self->InvokeEvent(vtkCommand::StartInteractionEvent, nullptr);
}

//------------------------------------------------------------------------------
void vtkBorderWidget::MoveAction(vtkAbstractWidget* w)
{
  vtkBorderWidget* self = reinterpret_cast<vtkBorderWidget*>(w);

  if (self->SubclassMoveAction())
  {
    return;
  }

  // compute some info we need for all cases
  int X = self->Interactor->GetEventPosition()[0];
  int Y = self->Interactor->GetEventPosition()[1];

  // Set the cursor appropriately
  if (self->WidgetState == vtkBorderWidget::Start)
  {
    int stateBefore = self->WidgetRep->GetInteractionState();
    self->WidgetRep->ComputeInteractionState(X, Y);
    int stateAfter = self->WidgetRep->GetInteractionState();
    self->SetCursor(stateAfter);

    vtkBorderRepresentation* borderRepresentation =
      reinterpret_cast<vtkBorderRepresentation*>(self->WidgetRep);
    if (self->Selectable || stateAfter != vtkBorderRepresentation::Inside)
    {
      borderRepresentation->MovingOff();
    }
    else
    {
      borderRepresentation->MovingOn();
    }

    if ((borderRepresentation->GetShowVerticalBorder() == vtkBorderRepresentation::BORDER_ACTIVE ||
          borderRepresentation->GetShowHorizontalBorder() ==
            vtkBorderRepresentation::BORDER_ACTIVE) &&
      stateBefore != stateAfter &&
      (stateBefore == vtkBorderRepresentation::Outside ||
        stateAfter == vtkBorderRepresentation::Outside))
    {
      self->Render();
    }
    return;
  }

  if (!self->Resizable && self->WidgetRep->GetInteractionState() != vtkBorderRepresentation::Inside)
  {
    return;
  }

  // Okay, adjust the representation (the widget is currently selected)
  double newEventPosition[2];
  newEventPosition[0] = static_cast<double>(X);
  newEventPosition[1] = static_cast<double>(Y);
  self->WidgetRep->WidgetInteraction(newEventPosition);

  // start a drag
  self->EventCallbackCommand->SetAbortFlag(1);
  self->InvokeEvent(vtkCommand::InteractionEvent, nullptr);
  self->Render();
}

//------------------------------------------------------------------------------
void vtkBorderWidget::EndSelectAction(vtkAbstractWidget* w)
{
  vtkBorderWidget* self = reinterpret_cast<vtkBorderWidget*>(w);

  if (self->SubclassEndSelectAction() ||
    self->WidgetRep->GetInteractionState() == vtkBorderRepresentation::Outside ||
    self->WidgetState != vtkBorderWidget::Selected)
  {
    return;
  }

  // Return state to not selected
  self->ReleaseFocus();
  self->WidgetState = vtkBorderWidget::Start;
  reinterpret_cast<vtkBorderRepresentation*>(self->WidgetRep)->MovingOff();

  // stop adjusting
  self->EventCallbackCommand->SetAbortFlag(1);
  self->EndInteraction();
  self->InvokeEvent(vtkCommand::EndInteractionEvent, nullptr);
}

//------------------------------------------------------------------------------
void vtkBorderWidget::HoverLeaveAction(vtkAbstractWidget* w)
{
  auto self = vtkBorderWidget::SafeDownCast(w);

  auto representation = self->GetBorderRepresentation();
  if (representation && representation->GetShowBorder() != vtkBorderRepresentation::BORDER_ON)
  {
    representation->SetBWActorDisplayOverlay(false);
    representation->SetInteractionState(vtkBorderRepresentation::Outside);
  }

  self->SetCursor(false);
  self->Render();
}

//------------------------------------------------------------------------------
void vtkBorderWidget::CreateDefaultRepresentation()
{
  if (!this->WidgetRep)
  {
    this->WidgetRep = vtkBorderRepresentation::New();
  }
}

//------------------------------------------------------------------------------
void vtkBorderWidget::SelectRegion(double* vtkNotUsed(eventPos[2]))
{
  this->InvokeEvent(vtkCommand::WidgetActivateEvent, nullptr);
}

//------------------------------------------------------------------------------
vtkTypeBool vtkBorderWidget::GetProcessEvents()
{
  auto representation = this->GetRepresentation();
  if (representation)
  {
    auto borderRepresentation = vtkBorderRepresentation::SafeDownCast(representation);
    if (borderRepresentation)
    {
      bool isRelativeLocation =
        borderRepresentation->GetWindowLocation() != vtkBorderRepresentation::AnyLocation;

      if (isRelativeLocation)
      {
        return false;
      }
    }
  }
  return this->Superclass::GetProcessEvents();
}

//------------------------------------------------------------------------------
void vtkBorderWidget::PrintSelf(ostream& os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);

  os << indent << "Selectable: " << (this->Selectable ? "On\n" : "Off\n");
  os << indent << "Resizable: " << (this->Resizable ? "On\n" : "Off\n");
}
