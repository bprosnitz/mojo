// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sky/engine/config.h"
#include "sky/engine/core/frame/NewEventHandler.h"

#include "sky/engine/core/dom/Document.h"
#include "sky/engine/core/dom/NodeRenderingTraversal.h"
#include "sky/engine/core/dom/TouchList.h"
#include "sky/engine/core/editing/Editor.h"
#include "sky/engine/core/editing/FrameSelection.h"
#include "sky/engine/core/editing/htmlediting.h"
#include "sky/engine/core/events/PointerEvent.h"
#include "sky/engine/core/frame/LocalFrame.h"
#include "sky/engine/core/frame/FrameView.h"
#include "sky/engine/core/page/EventWithHitTestResults.h"
#include "sky/engine/core/rendering/RenderObject.h"
#include "sky/engine/core/rendering/RenderView.h"
#include "sky/engine/platform/geometry/FloatPoint.h"
#include "sky/engine/public/platform/WebInputEvent.h"

namespace blink {

static VisiblePosition visiblePositionForHitTestResult(const HitTestResult& hitTestResult)
{
    Node* innerNode = hitTestResult.innerNode();
    VisiblePosition position(innerNode->renderer()->positionForPoint(hitTestResult.localPoint()));
    if (!position.isNull())
        return position;
    return VisiblePosition(firstPositionInOrBeforeNode(innerNode), DOWNSTREAM);
}

static LayoutPoint positionForEvent(const WebPointerEvent& event)
{
    return roundedLayoutPoint(FloatPoint(event.x, event.y));
}

NewEventHandler::NewEventHandler(LocalFrame& frame)
    : m_frame(frame)
{
}

NewEventHandler::~NewEventHandler()
{
}

Node* NewEventHandler::targetForHitTestResult(const HitTestResult& hitTestResult)
{
    Node* node = hitTestResult.innerNode();
    if (!node)
        return m_frame.document();
    if (node->isTextNode())
        return NodeRenderingTraversal::parent(node);
    return node;
}

HitTestResult NewEventHandler::performHitTest(const LayoutPoint& point)
{
    HitTestResult result(point);
    if (!m_frame.contentRenderer())
        return result;
    m_frame.contentRenderer()->hitTest(HitTestRequest(HitTestRequest::ReadOnly), result);
    return result;
}

bool NewEventHandler::dispatchPointerEvent(Node& target, const WebPointerEvent& event)
{
    RefPtr<PointerEvent> pointerEvent = PointerEvent::create(event);
    // TODO(abarth): Keep track of how many pointers are targeting the same node
    // and only mark the first one as primary.
    return target.dispatchEvent(pointerEvent.release());
}

bool NewEventHandler::dispatchClickEvent(Node& capturingTarget, const WebPointerEvent& event)
{
    ASSERT(event.type == WebInputEvent::PointerUp);
    HitTestResult hitTestResult = performHitTest(positionForEvent(event));
    Node* releaseTarget = targetForHitTestResult(hitTestResult);
    Node* clickTarget = NodeRenderingTraversal::commonAncestor(*releaseTarget, capturingTarget);
    if (!clickTarget)
        return true;
    // TODO(abarth): Make a proper gesture event that includes information from the event.
    return clickTarget->dispatchEvent(Event::createCancelableBubble(EventTypeNames::click));
}

void NewEventHandler::updateSelectionForPointerDown(const HitTestResult& hitTestResult, const WebPointerEvent& event)
{
    Node* innerNode = hitTestResult.innerNode();
    if (!innerNode->renderer())
        return;
    if (Position::nodeIsUserSelectNone(innerNode))
        return;
    if (!innerNode->dispatchEvent(Event::createCancelableBubble(EventTypeNames::selectstart)))
        return;
    VisiblePosition position = visiblePositionForHitTestResult(hitTestResult);
    // TODO(abarth): Can we change this to setSelectionIfNeeded?
    m_frame.selection().setNonDirectionalSelectionIfNeeded(VisibleSelection(position), CharacterGranularity);
}

bool NewEventHandler::handlePointerEvent(const WebPointerEvent& event)
{
    if (event.type == WebInputEvent::PointerDown)
        return handlePointerDownEvent(event);
    if (event.type == WebInputEvent::PointerUp)
        return handlePointerUpEvent(event);
    if (event.type == WebInputEvent::PointerMove)
        return handlePointerMoveEvent(event);
    ASSERT(event.type == WebInputEvent::PointerCancel);
    return handlePointerCancelEvent(event);
}

bool NewEventHandler::handlePointerDownEvent(const WebPointerEvent& event)
{
    ASSERT(!m_targetForPointer.contains(event.pointer));
    HitTestResult hitTestResult = performHitTest(positionForEvent(event));
    RefPtr<Node> target = targetForHitTestResult(hitTestResult);
    m_targetForPointer.set(event.pointer, target);
    bool eventSwallowed = !dispatchPointerEvent(*target, event);
    // TODO(abarth): Set the target for the pointer to something determined when
    // dispatching the event.
    updateSelectionForPointerDown(hitTestResult, event);
    return eventSwallowed;
}

bool NewEventHandler::handlePointerUpEvent(const WebPointerEvent& event)
{
    RefPtr<Node> target = m_targetForPointer.take(event.pointer);
    if (!target)
        return false;
    bool eventSwallowed = !dispatchPointerEvent(*target, event);
    // When the user releases the primary pointer, we need to dispatch a tap
    // event to the common ancestor for where the pointer went down and where
    // it came up.
    if (!dispatchClickEvent(*target, event))
        eventSwallowed = true;
    return eventSwallowed;
}

bool NewEventHandler::handlePointerMoveEvent(const WebPointerEvent& event)
{
    RefPtr<Node> target = m_targetForPointer.get(event.pointer);
    return target && dispatchPointerEvent(*target.get(), event);
}

bool NewEventHandler::handlePointerCancelEvent(const WebPointerEvent& event)
{
    RefPtr<Node> target = m_targetForPointer.take(event.pointer);
    return target && dispatchPointerEvent(*target, event);
}

}