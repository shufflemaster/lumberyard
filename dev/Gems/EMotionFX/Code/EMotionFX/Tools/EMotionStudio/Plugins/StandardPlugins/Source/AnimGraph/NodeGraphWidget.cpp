/*
* All or portions of this file Copyright (c) Amazon.com, Inc. or its affiliates or
* its licensors.
*
* For complete copyright and license terms please see the LICENSE at the root of this
* distribution (the "License"). All use of this software is governed by the License,
* or, if provided, by the license below or the license accompanying this file. Do not
* remove or modify any license notices. This file is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*
*/

// include the required headers
#include "NodeGraphWidget.h"
#include "AnimGraphPlugin.h"
#include "NodeGraph.h"
#include "BlendTreeVisualNode.h"
#include "GraphNode.h"
#include "StateGraphNode.h"
#include "GraphWidgetCallback.h"
#include "NodeConnection.h"
#include "BlendGraphWidget.h"
#include "../../../../EMStudioSDK/Source/MainWindow.h"
#include <MysticQt/Source/KeyboardShortcutManager.h>

#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QMouseEvent>

#include <MCore/Source/LogManager.h>

#include <EMotionFX/CommandSystem/Source/AnimGraphConnectionCommands.h>
#include <EMotionFX/Source/ActorManager.h>


namespace EMStudio
{
    // the constructor
    //NodeGraphWidget::NodeGraphWidget(NodeGraph* activeGraph, QWidget* parent) : QGLWidget(QGLFormat(QGL::SampleBuffers), parent)
    NodeGraphWidget::NodeGraphWidget(AnimGraphPlugin* plugin, NodeGraph* activeGraph, QWidget* parent)
        : QOpenGLWidget(parent)
        , QOpenGLFunctions()
    {
        setObjectName("NodeGraphWidget");

        mPlugin = plugin;
        mFontMetrics = new QFontMetrics(mFont);

        // update the active graph
        SetActiveGraph(activeGraph);

        // enable mouse tracking to capture mouse movements over the widget
        setMouseTracking(true);

        // init members
        mShowFPS            = false;
        mLeftMousePressed   = false;
        mPanning            = false;
        mMiddleMousePressed = false;
        mRightMousePressed  = false;
        mRectSelecting      = false;
        mShiftPressed       = false;
        mControlPressed     = false;
        mAltPressed         = false;
        mMoveNode           = nullptr;
        mMouseLastPos       = QPoint(0, 0);
        mMouseLastPressPos  = QPoint(0, 0);
        mMousePos           = QPoint(0, 0);
        mCallback           = nullptr;

        // setup to get focus when we click or use the mouse wheel
        setFocusPolicy((Qt::FocusPolicy)(Qt::ClickFocus | Qt::WheelFocus));

        // accept drops
        setAcceptDrops(true);

        setAutoFillBackground(false);
        setAttribute(Qt::WA_OpaquePaintEvent);

        mCurWidth       = geometry().width();
        mCurHeight      = geometry().height();
        mPrevWidth      = mCurWidth;
        mPrevHeight     = mCurHeight;
    }


    // destructor
    NodeGraphWidget::~NodeGraphWidget()
    {
        delete mCallback;

        // delete the overlay font metrics
        delete mFontMetrics;
    }


    // initialize opengl
    void NodeGraphWidget::initializeGL()
    {
        initializeOpenGLFunctions();
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    }


    //
    void NodeGraphWidget::resizeGL(int w, int h)
    {
        static QPoint sizeDiff(0, 0);

        mCurWidth = w;
        mCurHeight = h;

        // specify the center of the window, so that that is the origin
        if (mActiveGraph)
        {
            mActiveGraph->SetScalePivot(QPoint(w / 2, h / 2));

            QPoint  scrollOffset    = mActiveGraph->GetScrollOffset();
            int32   scrollOffsetX   = scrollOffset.x();
            int32   scrollOffsetY   = scrollOffset.y();

            // calculate the size delta
            QPoint  oldSize         = QPoint(mPrevWidth, mPrevHeight);
            QPoint  size            = QPoint(w, h);
            QPoint  diff            = oldSize - size;
            sizeDiff += diff;

            const int32 sizeDiffX       = sizeDiff.x();
            const int32 halfSizeDiffX   = sizeDiffX / 2;

            const int32 sizeDiffY       = sizeDiff.y();
            const int32 halfSizeDiffY   = sizeDiffY / 2;

            if (halfSizeDiffX != 0)
            {
                scrollOffsetX -= halfSizeDiffX;
                const int32 modRes = halfSizeDiffX % 2;
                sizeDiff.setX(modRes);
            }

            if (halfSizeDiffY != 0)
            {
                scrollOffsetY -= halfSizeDiffY;
                const int32 modRes = halfSizeDiffY % 2;
                sizeDiff.setY(modRes);
            }

            mActiveGraph->SetScrollOffset(QPoint(scrollOffsetX, scrollOffsetY));
            //MCore::LOG("%d, %d", scrollOffsetX, scrollOffsetY);
        }

        QOpenGLWidget::resizeGL(w, h);

        mPrevWidth = w;
        mPrevHeight = h;
    }


    // set the active graph
    void NodeGraphWidget::SetActiveGraph(NodeGraph* graph)
    {
        mActiveGraph = graph;
        mMoveNode = nullptr;
    }


    // get the active graph
    NodeGraph* NodeGraphWidget::GetActiveGraph()
    {
        return mActiveGraph;
    }


    // paint event
    void NodeGraphWidget::paintGL()
    {
        if (visibleRegion().isEmpty())
        {
            return;
        }

        if (PreparePainting() == false)
        {
            return;
        }

        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // calculate the time passed since the last render
        const float timePassedInSeconds = mRenderTimer.StampAndGetDeltaTimeInSeconds();

        // start painting
        QPainter painter(this);

        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::HighQualityAntialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        // get the width and height
        const uint32 width  = mCurWidth;
        const uint32 height = mCurHeight;

        // fill the background
        //painter.fillRect( event->rect(), QColor(30, 30, 30) );
        painter.setBrush(QColor(47, 47, 47));
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRect(0, 0, width, height));

        // render the active graph
        if (mActiveGraph)
        {
            mActiveGraph->Render(painter, width, height, mMousePos, timePassedInSeconds);
        }

        // render selection rect
        if (mRectSelecting)
        {
            painter.resetTransform();
            QRect selectRect;
            CalcSelectRect(selectRect);

            if (mAltPressed)
            {
                painter.setBrush(QColor(0, 100, 200, 75));
                painter.setPen(QColor(0, 100, 255));
            }
            else
            {
                painter.setBrush(QColor(200, 120, 0, 75));
                painter.setPen(QColor(255, 128, 0));
            }

            painter.drawRect(selectRect);
        }

        // draw the overlay
        OnDrawOverlay(painter);

        // render the callback overlay
        if (mCallback)
        {
            painter.resetTransform();
            mCallback->DrawOverlay(painter);
        }

        // draw the border
        QColor borderColor(0, 0, 0);
        if (hasFocus())
        {
            borderColor = QColor(244, 156, 28);
        }

        QPen pen(borderColor, hasFocus() ? 3 : 2);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);

        painter.resetTransform();
        painter.drawLine(0, 0, width, 0);
        painter.drawLine(width, 0, width, height);
        painter.drawLine(0, 0, 0, height);
        painter.drawLine(0, height, width, height);

        //painter.setPen( Qt::NoPen );
        //painter.setBrush( Qt::NoBrush );
        //painter.setBackgroundMode(Qt::TransparentMode);

        // render FPS counter
        if (mShowFPS)
        {
            // get the time delta between the current time and the last frame
            static AZ::Debug::Timer perfTimer;
            static float perfTimeDelta = perfTimer.StampAndGetDeltaTimeInSeconds();
            perfTimeDelta = perfTimer.StampAndGetDeltaTimeInSeconds();

            static float fpsTimeElapsed = 0.0f;
            static uint32 fpsNumFrames = 0;
            static uint32 lastFPS = 0;
            fpsTimeElapsed += perfTimeDelta;
            const float renderTime = mRenderTimer.StampAndGetDeltaTimeInSeconds() * 1000.0f;
            fpsNumFrames++;
            if (fpsTimeElapsed > 1.0f)
            {
                lastFPS         = fpsNumFrames;
                fpsTimeElapsed  = 0.0f;
                fpsNumFrames    = 0;
            }

            static AZStd::string perfTempString;
            float theoreticalFPS = 1000.0f / renderTime;
            perfTempString = AZStd::string::format("%i FPS - %.0fms (%i FPS)", lastFPS, renderTime, (int)theoreticalFPS);

            GraphNode::RenderText(painter, perfTempString.c_str(), QColor(150, 150, 150), mFont, *mFontMetrics, Qt::AlignRight, QRect(width - 55, height - 20, 50, 20));
        }

        // show the info to which actor the currently rendered graph belongs to
        const CommandSystem::SelectionList& selectionList = CommandSystem::GetCommandManager()->GetCurrentSelection();

        if ((EMotionFX::GetActorManager().GetNumActorInstances() > 1) && (selectionList.GetNumSelectedActorInstances() > 0))
        {
            // get the first of the multiple selected actor instances
            EMotionFX::ActorInstance* firstActorInstance = selectionList.GetFirstActorInstance();

            // update the stored short filename without path
            if (mFullActorName != firstActorInstance->GetActor()->GetFileName())
            {
                AzFramework::StringFunc::Path::GetFileName(firstActorInstance->GetActor()->GetFileNameString().c_str(), mActorName);
            }

            mTempString = AZStd::string::format("Showing graph for ActorInstance with ID %d and Actor file \"%s\"", firstActorInstance->GetID(), mActorName.c_str());
            GraphNode::RenderText(painter, mTempString.c_str(), QColor(150, 150, 150), mFont, *mFontMetrics, Qt::AlignLeft, QRect(8, 0, 50, 20));
        }
    }


    // convert to a global position
    QPoint NodeGraphWidget::LocalToGlobal(const QPoint& inPoint)
    {
        if (mActiveGraph)
        {
            return mActiveGraph->GetTransform().inverted().map(inPoint);
        }

        return inPoint;
    }


    // convert to a local position
    QPoint NodeGraphWidget::GlobalToLocal(const QPoint& inPoint)
    {
        if (mActiveGraph)
        {
            return mActiveGraph->GetTransform().map(inPoint);
        }

        return inPoint;
    }


    QPoint NodeGraphWidget::SnapLocalToGrid(const QPoint& inPoint, uint32 cellSize)
    {
        MCORE_UNUSED(cellSize);

        //QPoint scaledMouseDelta = (mousePos - mMouseLastPos) * (1.0f / mActiveGraph->GetScale());
        //QPoint unSnappedTopRight = oldTopRight + scaledMouseDelta;
        QPoint snapped;
        snapped.setX(inPoint.x() - MCore::Math::FMod(inPoint.x(), 10.0f));
        snapped.setY(inPoint.y() - MCore::Math::FMod(inPoint.y(), 10.0f));
        //snapDelta = snappedTopRight - unSnappedTopRight;
        return snapped;
    }

    // mouse is moving over the widget
    void NodeGraphWidget::mouseMoveEvent(QMouseEvent* event)
    {
        if (mActiveGraph == nullptr)
        {
            return;
        }

        // get the mouse position, calculate the global mouse position and update the relevant data
        QPoint mousePos = event->pos();

        QPoint snapDelta(0, 0);
        if (mMoveNode && mLeftMousePressed && mPanning == false && mRectSelecting == false)
        {
            QPoint oldTopRight = mMoveNode->GetRect().topRight();
            QPoint scaledMouseDelta = (mousePos - mMouseLastPos) * (1.0f / mActiveGraph->GetScale());
            QPoint unSnappedTopRight = oldTopRight + scaledMouseDelta;
            QPoint snappedTopRight = SnapLocalToGrid(unSnappedTopRight, 10);
            snapDelta = snappedTopRight - unSnappedTopRight;
        }

        mousePos += snapDelta * mActiveGraph->GetScale();
        QPoint delta        = (mousePos - mMouseLastPos) * (1.0f / mActiveGraph->GetScale());
        mMouseLastPos       = mousePos;
        QPoint globalPos    = LocalToGlobal(mousePos);
        SetMousePos(globalPos);

        //if (delta.x() > 0 || delta.x() < -0 || delta.y() > 0 || delta.y() < -0)
        if (delta.x() != 0 || delta.y() != 0)
        {
            mAllowContextMenu = false;
        }

        // update modifiers
        mAltPressed     = event->modifiers() & Qt::AltModifier;
        mShiftPressed   = event->modifiers() & Qt::ShiftModifier;
        mControlPressed = event->modifiers() & Qt::ControlModifier;

        /*GraphNode* node = */ UpdateMouseCursor(mousePos, globalPos);
        if (mRectSelecting == false && mMoveNode == nullptr)
        {
            // check if we are clicking on a port
            GraphNode*  portNode    = nullptr;
            NodePort*   port        = nullptr;
            uint32      portNr      = MCORE_INVALIDINDEX32;
            bool        isInputPort = true;
            port = mActiveGraph->FindPort(globalPos.x(), globalPos.y(), &portNode, &portNr, &isInputPort);

            // check if we are adjusting a transition head or tail
            if (mActiveGraph->GetIsRepositioningTransitionHead() || mActiveGraph->GetIsRepositioningTransitionTail())
            {
                NodeConnection* connection = mActiveGraph->GetRepositionedTransitionHead();
                if (connection == nullptr)
                {
                    connection = mActiveGraph->GetRepositionedTransitionTail();
                }

                MCORE_ASSERT(connection->GetType() == StateConnection::TYPE_ID);
                StateConnection* stateConnection = static_cast<StateConnection*>(connection);

                // check if our mouse is-over a node
                GraphNode* hoveredNode = mActiveGraph->FindNode(mousePos);
                if (hoveredNode == nullptr && portNode)
                {
                    hoveredNode = portNode;
                }

                if (mActiveGraph->GetIsRepositioningTransitionHead())
                {
                    // when adjusting the arrow head and we are over the source node with the mouse
                    if (hoveredNode
                        && hoveredNode != stateConnection->GetSourceNode()
                        && CheckIfIsValidTransition(stateConnection->GetSourceNode(), hoveredNode))
                    {
                        stateConnection->SetTargetNode(hoveredNode); 
                        mActiveGraph->SetReplaceTransitionValid(true);
                    }
                    else
                    {
                        mActiveGraph->SetReplaceTransitionValid(false);
                    }

                    GraphNode* targetNode = stateConnection->GetTargetNode();
                    if (targetNode)
                    {
                        QPoint offset = globalPos - targetNode->GetRect().topLeft();
                        stateConnection->SetEndOffset(offset);
                    }
                }
                else if (mActiveGraph->GetIsRepositioningTransitionTail())
                {
                    // when adjusting the arrow tail and we are over the target node with the mouse
                    if (hoveredNode
                        && hoveredNode != stateConnection->GetTargetNode()
                        && CheckIfIsValidTransition(hoveredNode, stateConnection->GetTargetNode()))
                    {
                        stateConnection->SetSourceNode(hoveredNode);
                        mActiveGraph->SetReplaceTransitionValid(true);
                    }
                    else
                    {
                        mActiveGraph->SetReplaceTransitionValid(false);
                    }

                    GraphNode* sourceNode = stateConnection->GetSourceNode();
                    if (sourceNode)
                    {
                        QPoint offset = globalPos - sourceNode->GetRect().topLeft();
                        stateConnection->SetStartOffset(offset);
                    }
                    
                }
            }

            // connection relinking or creation
            if (port)
            {
                if (mActiveGraph->GetIsCreatingConnection())
                {
                    const bool isValid = CheckIfIsCreateConnectionValid(portNr, portNode, port, isInputPort);
                    mActiveGraph->SetCreateConnectionIsValid(isValid);
                    mActiveGraph->SetTargetPort(port);
                    //update();
                    return;
                }
                else if (mActiveGraph->GetIsRelinkingConnection())
                {
                    bool isValid = BlendGraphWidget::CheckIfIsRelinkConnectionValid(mActiveGraph->GetRelinkConnection(), portNode, portNr, isInputPort);
                    mActiveGraph->SetCreateConnectionIsValid(isValid);
                    mActiveGraph->SetTargetPort(port);
                    //update();
                    return;
                }
                else
                {
                    if ((isInputPort && portNode->GetCreateConFromOutputOnly() == false) || isInputPort == false)
                    {
                        UpdateMouseCursor(mousePos, globalPos);
                        //update();
                        return;
                    }
                }
            }
            else
            {
                mActiveGraph->SetTargetPort(nullptr);
            }
        }

        // if we are panning
        if (mPanning)
        {
            // handle mouse wrapping, to enable smoother panning
            bool mouseWrapped = false;
            if (event->x() > (int32)width())
            {
                mouseWrapped = true;
                QCursor::setPos(QPoint(event->globalX() - width(), event->globalY()));
                mMouseLastPos = QPoint(event->x() - width(), event->y());
            }
            else if (event->x() < 0)
            {
                mouseWrapped = true;
                QCursor::setPos(QPoint(event->globalX() + width(), event->globalY()));
                mMouseLastPos = QPoint(event->x() + width(), event->y());
            }

            if (event->y() > (int32)height())
            {
                mouseWrapped = true;
                QCursor::setPos(QPoint(event->globalX(), event->globalY() - height()));
                mMouseLastPos = QPoint(event->x(), event->y() - height());
            }
            else if (event->y() < 0)
            {
                mouseWrapped = true;
                QCursor::setPos(QPoint(event->globalX(), event->globalY() + height()));
                mMouseLastPos = QPoint(event->x(), event->y() + height());
            }

            // don't apply the delta, if mouse has been wrapped
            if (mouseWrapped)
            {
                delta = QPoint(0, 0);
            }

            if (mActiveGraph)
            {
                // scrolling
                if (mAltPressed == false)
                {
                    QPoint newOffset = mActiveGraph->GetScrollOffset();
                    newOffset += delta;
                    mActiveGraph->SetScrollOffset(newOffset);
                    mActiveGraph->StopAnimatedScroll();
                    UpdateMouseCursor(mousePos, globalPos);
                    //update();
                    return;
                }
                // zooming
                else
                {
                    // stop the automated zoom
                    mActiveGraph->StopAnimatedZoom();

                    // calculate the new scale value
                    const float scaleDelta = (delta.y() / 120.0f) * 0.2f;
                    float newScale = MCore::Clamp<float>(mActiveGraph->GetScale() + scaleDelta, mActiveGraph->GetLowestScale(), 1.0f);
                    mActiveGraph->SetScale(newScale);

                    // redraw the viewport
                    //update();
                }
            }
        }

        // if the left mouse button is pressed
        if (mLeftMousePressed)
        {
            if (mMoveNode)
            {
                if (mActiveGraph)
                {
                    if (mActiveGraph->CalcNumSelectedNodes() > 0)
                    {
                        // move all selected nodes
                        const uint32 numNodes = mActiveGraph->GetNumNodes();
                        for (uint32 i = 0; i < numNodes; ++i)
                        {
                            if (mActiveGraph->GetNode(i)->GetIsSelected())
                            {
                                mActiveGraph->GetNode(i)->MoveRelative(delta);
                            }
                        }

                        return;
                    }
                    else
                    {
                        if (mMoveNode)
                        {
                            mMoveNode->MoveRelative(delta);
                        }

                        return;
                    }
                }
            }
            else
            {
                //setCursor( Qt::ArrowCursor );

                if (mRectSelecting)
                {
                    mSelectEnd = mousePos;
                }
            }

            // redraw viewport
            //update();
            return;
        }

        // if we're just moving around without pressing the left mouse button, change the cursor
        //UpdateMouseCursor( mousePos, globalPos );
        //update();
    }


    // mouse button has been pressed
    void NodeGraphWidget::mousePressEvent(QMouseEvent* event)
    {
        if (mActiveGraph == nullptr)
        {
            return;
        }

        GetMainWindow()->DisableUndoRedo();

        mAllowContextMenu = true;

        // get the mouse position, calculate the global mouse position and update the relevant data
        QPoint mousePos     = event->pos();
        mMouseLastPos       = mousePos;
        mMouseLastPressPos  = mousePos;
        QPoint globalPos    = LocalToGlobal(mousePos);
        SetMousePos(globalPos);

        // update modifiers
        mAltPressed     = event->modifiers() & Qt::AltModifier;
        mShiftPressed   = event->modifiers() & Qt::ShiftModifier;
        mControlPressed = event->modifiers() & Qt::ControlModifier;

        // check if we can start panning
        if ((event->buttons() & Qt::RightButton && event->buttons() & Qt::LeftButton) || event->button() == Qt::RightButton || event->button() == Qt::MidButton)
        {
            // update button booleans
            if (event->buttons() & Qt::RightButton && event->buttons() & Qt::LeftButton)
            {
                mLeftMousePressed  = true;
                mRightMousePressed = true;
            }

            if (event->button() == Qt::RightButton)
            {
                mRightMousePressed = true;

                GraphNode* node = UpdateMouseCursor(mousePos, globalPos);
                if (node && node->GetCanVisualize() && node->GetIsInsideVisualizeRect(globalPos))
                {
                    OnSetupVisualizeOptions(node);
                    mRectSelecting = false;
                    mPanning = false;
                    mMoveNode = nullptr;
                    //update();
                    return;
                }
            }

            if (event->button() == Qt::MidButton)
            {
                mMiddleMousePressed = true;
            }

            mPanning = true;
            mRectSelecting = false;
            setCursor(Qt::ClosedHandCursor);
            //mMoveNode = nullptr;

            // update viewport and return
            //update();
            return;
        }

        // if we press the left mouse button
        if (event->button() == Qt::LeftButton)
        {
            mLeftMousePressed = true;

            // get the node we click on
            GraphNode* node = UpdateMouseCursor(mousePos, globalPos);

            // if we pressed the visualize icon
            GraphNode* orgNode = mActiveGraph->FindNode(mousePos);
            if (orgNode && orgNode->GetCanVisualize() && orgNode->GetIsInsideVisualizeRect(globalPos))
            {
                const bool viz = !orgNode->GetIsVisualized();
                orgNode->SetIsVisualized(viz);
                OnVisualizeToggle(orgNode, viz);
                mRectSelecting = false;
                mPanning = false;
                mMoveNode = nullptr;
                //update();
                return;
            }

            // collapse the node if possible (not possible in a state machine)
            if (node)
            {
                // cast the node
                BlendTreeVisualNode* blendNode = static_cast<BlendTreeVisualNode*>(node);
                EMotionFX::AnimGraphNode* animGraphNode = blendNode->GetEMFXNode();

                // check if the node is not part of a state machine, this is the only case where the collapsible node is possible
                if (azrtti_typeid(animGraphNode->GetParentNode()) != azrtti_typeid<EMotionFX::AnimGraphStateMachine>())
                {
                    if (node->GetIsInsideArrowRect(globalPos))
                    {
                        node->SetIsCollapsed(!node->GetIsCollapsed());
                        OnNodeCollapsed(node, node->GetIsCollapsed());
                        UpdateMouseCursor(mousePos, globalPos);
                        //update();
                        return;
                    }
                }
            }

            // check if we are clicking on an input port
            GraphNode*  portNode    = nullptr;
            NodePort*   port        = nullptr;
            uint32      portNr      = MCORE_INVALIDINDEX32;
            bool        isInputPort = true;
            port = mActiveGraph->FindPort(globalPos.x(), globalPos.y(), &portNode, &portNr, &isInputPort);
            if (port)
            {
                mMoveNode       = nullptr;
                mPanning        = false;
                mRectSelecting  = false;

                // relink existing connection
                NodeConnection* connection = mActiveGraph->FindInputConnection(portNode, portNr);
                if (isInputPort && connection && portNode->GetType() != StateGraphNode::TYPE_ID)
                {
                    //connection->SetColor();
                    //MCore::LOG("%s(%i)->%s(%i)", connection->GetSourceNode()->GetName(), connection->GetOutputPortNr(), connection->GetTargetNode()->GetName(), connection->GetInputPortNr());
                    //GraphNode*    createConNode   = connection->GetSourceNode();
                    //uint32        createConPortNr = connection->GetOutputPortNr();
                    //NodePort* createConPort   = createConNode->GetOutputPort( createConPortNr );
                    //QPoint        createConOffset = QPoint(0,0);//globalPos - createConNode->GetRect().topLeft();
                    connection->SetIsDashed(true);

                    UpdateMouseCursor(mousePos, globalPos);
                    //mActiveGraph->StartCreateConnection( createConPortNr, !isInputPort, createConNode, createConPort, createConOffset );
                    mActiveGraph->StartRelinkConnection(connection, portNr, portNode);
                    //update();
                    return;
                }

                // create new connection
                if ((isInputPort && portNode->GetCreateConFromOutputOnly() == false) || isInputPort == false)
                {
                    if (CheckIfIsValidTransitionSource(portNode))
                    {
                        QPoint offset = globalPos - portNode->GetRect().topLeft();
                        UpdateMouseCursor(mousePos, globalPos);
                        mActiveGraph->StartCreateConnection(portNr, isInputPort, portNode, port, offset);
                        //update();
                        return;
                    }
                }

                //          update();
            }

            // check if we click on an transition arrow head or tail
            NodeConnection* connection = mActiveGraph->FindConnection(globalPos);
            if (connection && connection->GetType() == StateConnection::TYPE_ID)
            {
                StateConnection* stateConnection = static_cast<StateConnection*>(connection);

                if (stateConnection->GetIsWildcardTransition() == false && stateConnection->CheckIfIsCloseToHead(globalPos))
                {
                    mMoveNode       = nullptr;
                    mPanning        = false;
                    mRectSelecting  = false;

                    mActiveGraph->StartReplaceTransitionHead(stateConnection, stateConnection->GetStartOffset(), stateConnection->GetEndOffset(), stateConnection->GetSourceNode(), stateConnection->GetTargetNode());

                    //update();
                    return;
                }

                if (stateConnection->GetIsWildcardTransition() == false && stateConnection->CheckIfIsCloseToTail(globalPos))
                {
                    mMoveNode       = nullptr;
                    mPanning        = false;
                    mRectSelecting  = false;

                    mActiveGraph->StartReplaceTransitionTail(stateConnection, stateConnection->GetStartOffset(), stateConnection->GetEndOffset(), stateConnection->GetSourceNode(), stateConnection->GetTargetNode());

                    //update();
                    return;
                }
            }

            // get the node we click on
            node = UpdateMouseCursor(mousePos, globalPos);
            if (node && mShiftPressed)
            {
                OnShiftClickedNode(node);
            }
            else
            {
                if (node && mShiftPressed == false && mControlPressed == false && mAltPressed == false)
                {
                    mMoveNode   = node;
                    mPanning    = false;
                    setCursor(Qt::ClosedHandCursor);
                }
                else
                {
                    mMoveNode       = nullptr;
                    mPanning        = false;
                    mRectSelecting  = true;
                    mSelectStart    = mousePos;
                    mSelectEnd      = mSelectStart;
                    setCursor(Qt::ArrowCursor);
                }
            }

            // selection handling
            if (mActiveGraph)
            {
                bool selectionChanged = false;

                // check if we clicked a node and additionally not clicked its arrow rect
                bool nodeClicked = false;
                if (node && node->GetIsInsideArrowRect(globalPos) == false)
                {
                    nodeClicked = true;
                }

                // shift is used to activate a state, disable all selection behaviour in case we press shift!
                if (mShiftPressed == false)
                {
                    // check the node we're clicking on
                    if (mControlPressed == false)
                    {
                        // only reset the selection in case we clicked in empty space or in case the node we clicked on is not part of
                        if (node == nullptr || (node && node->GetIsSelected() == false))
                        {
                            mActiveGraph->UnselectAllNodes(true);
                            selectionChanged = true;
                        }
                    }

                    // node clicked with shift only
                    if (nodeClicked && mControlPressed)
                    {
                        node->ToggleSelected();
                        selectionChanged = true;
                    }
                    // node clicked with ctrl only
                    else if (nodeClicked && mControlPressed == false)
                    {
                        node->SetIsSelected(true);
                        selectionChanged = true;
                    }
                    // in case we didn't click on a node, check if we click on a connection
                    else if (nodeClicked == false)
                    {
                        mActiveGraph->SelectConnectionCloseTo(LocalToGlobal(event->pos()), mControlPressed == false, true);
                    }

                    // trigger the selection change callback
                    if (selectionChanged)
                    {
                        OnSelectionChanged();
                    }
                }
                else
                {
                    // in case shift and control are both pressed, special case!
                    if (mControlPressed)
                    {
                        mActiveGraph->UnselectAllNodes(true);

                        if (node)
                        {
                            node->SetIsSelected(true);
                        }

                        OnSelectionChanged();
                    }
                }
            }
        }

        // redraw the viewport
        //update();
    }


    // mouse button has been released
    void NodeGraphWidget::mouseReleaseEvent(QMouseEvent* event)
    {
        // get the mouse position, calculate the global mouse position and update the relevant data
        const QPoint mousePos   = event->pos();
        const QPoint globalPos  = LocalToGlobal(mousePos);
        SetMousePos(globalPos);

        if (mActiveGraph == nullptr)
        {
            return;
        }

        GetMainWindow()->UpdateUndoRedo();

        // update modifiers
        mAltPressed     = event->modifiers() & Qt::AltModifier;
        mShiftPressed   = event->modifiers() & Qt::ShiftModifier;
        mControlPressed = event->modifiers() & Qt::ControlModifier;

        // both left and right released at the same time
        if (event->buttons() & Qt::RightButton && event->buttons() & Qt::LeftButton)
        {
            mRightMousePressed  = false;
            mLeftMousePressed   = false;
        }

        // right mouse button
        if (event->button() == Qt::RightButton)
        {
            mRightMousePressed  = false;
            mPanning            = false;
            UpdateMouseCursor(mousePos, globalPos);
            //update();
            return;
        }

        // middle mouse button
        if (event->button() == Qt::MidButton)
        {
            mMiddleMousePressed = false;
            mPanning            = false;
        }

        // if we release the left mouse button
        if (event->button() == Qt::LeftButton)
        {
            const bool mouseMoved = (event->pos() != mMouseLastPressPos);

            // if we pressed the visualize icon
            GraphNode* node = UpdateMouseCursor(mousePos, globalPos);
            if (node && node->GetCanVisualize() && node->GetIsInsideVisualizeRect(globalPos))
            {
                mRectSelecting = false;
                mPanning = false;
                mMoveNode = nullptr;
                mLeftMousePressed = false;
                UpdateMouseCursor(mousePos, globalPos);
                //update();
                return;
            }

            if (node && node->GetIsInsideArrowRect(globalPos))
            {
                mRectSelecting = false;
                mPanning = false;
                mMoveNode = nullptr;
                mLeftMousePressed = false;
                UpdateMouseCursor(mousePos, globalPos);
                //update();
                return;
            }

            // if we were creating a connection
            if (mActiveGraph->GetIsCreatingConnection())
            {
                // create the connection if needed
                if (mActiveGraph->GetTargetPort())
                {
                    if (mActiveGraph->GetIsCreateConnectionValid())
                    {
                        uint32      targetPortNr;
                        bool        targetIsInputPort;
                        GraphNode*  targetNode;

                        NodePort* targetPort = mActiveGraph->FindPort(globalPos.x(), globalPos.y(), &targetNode, &targetPortNr, &targetIsInputPort);
                        if (targetPort && mActiveGraph->GetTargetPort() == targetPort
                            && targetNode != mActiveGraph->GetCreateConnectionNode())
                        {
#ifndef MCORE_DEBUG
                            MCORE_UNUSED(targetPort);
#endif

                            QPoint endOffset = globalPos - targetNode->GetRect().topLeft();
                            mActiveGraph->SetCreateConnectionEndOffset(endOffset);

                            // trigger the callback
                            OnCreateConnection(mActiveGraph->GetCreateConnectionPortNr(),
                                mActiveGraph->GetCreateConnectionNode(),
                                mActiveGraph->GetCreateConnectionIsInputPort(),
                                targetPortNr,
                                targetNode,
                                targetIsInputPort,
                                mActiveGraph->GetCreateConnectionStartOffset(),
                                mActiveGraph->GetCreateConnectionEndOffset());
                        }
                    }
                }

                mActiveGraph->StopCreateConnection();
                mLeftMousePressed = false;
                UpdateMouseCursor(mousePos, globalPos);
                //update();
                return;
            }

            // if we were relinking a connection
            if (mActiveGraph->GetIsRelinkingConnection())
            {
                // get the information from the current mouse position
                uint32      newTargetPortNr;
                bool        newTargetIsInputPort;
                GraphNode*  newTargetNode;
                NodePort*   newTargetPort = mActiveGraph->FindPort(globalPos.x(), globalPos.y(), &newTargetNode, &newTargetPortNr, &newTargetIsInputPort);

                // relink existing connection
                NodeConnection* connection = nullptr;
                if (newTargetPort)
                {
                    connection = mActiveGraph->FindInputConnection(newTargetNode, newTargetPortNr);
                }

                NodeConnection* relinkedConnection = mActiveGraph->GetRelinkConnection();
                if (relinkedConnection)
                {
                    relinkedConnection->SetIsDashed(false);
                }

                if (newTargetNode && newTargetPort)
                {
                    // get the information from the old connection which we want to relink
                    GraphNode*      sourceNode          = relinkedConnection->GetSourceNode();
                    AZStd::string   sourceNodeName      = sourceNode->GetName();
                    uint32          sourcePortNr        = relinkedConnection->GetOutputPortNr();
                    GraphNode*      oldTargetNode       = relinkedConnection->GetTargetNode();
                    AZStd::string   oldTargetNodeName   = oldTargetNode->GetName();
                    uint32          oldTargetPortNr     = relinkedConnection->GetInputPortNr();

                    if (BlendGraphWidget::CheckIfIsRelinkConnectionValid(relinkedConnection, newTargetNode, newTargetPortNr, newTargetIsInputPort))
                    {
                        // get the first selected anim graph
                        EMotionFX::AnimGraph* animGraph = mPlugin->GetActiveAnimGraph();
                        MCORE_ASSERT(animGraph);

                        // create the relink command group
                        MCore::CommandGroup commandGroup("Relink blend tree connection");

                        // check if there already is a connection plugged into the port where we want to put our new connection in
                        if (connection)
                        {
                            // construct the command
                            AZStd::string commandString;
                            commandString = AZStd::string::format("AnimGraphRemoveConnection -animGraphID %i -sourceNode \"%s\" -sourcePort %d -targetNode \"%s\" -targetPort %d",
                                animGraph->GetID(),
                                connection->GetSourceNode()->GetName(),
                                connection->GetOutputPortNr(),
                                connection->GetTargetNode()->GetName(),
                                connection->GetInputPortNr());

                            // add it to the command group
                            commandGroup.AddCommandString(commandString.c_str());
                        }

                        MCORE_ASSERT(newTargetIsInputPort);
                        AZStd::string newTargetNodeName = newTargetNode->GetName();
                        CommandSystem::RelinkConnectionTarget(&commandGroup, animGraph->GetID(), sourceNodeName.c_str(), sourcePortNr, oldTargetNodeName.c_str(), oldTargetPortNr, newTargetNodeName.c_str(), newTargetPortNr);

                        // call this before calling the commands as the commands will trigger a graph update
                        mActiveGraph->StopRelinkConnection();

                        // execute the command group
                        AZStd::string commandResult;
                        if (GetCommandManager()->ExecuteCommandGroup(commandGroup, commandResult) == false)
                        {
                            if (commandResult.empty() == false)
                            {
                                MCore::LogError(commandResult.c_str());
                            }
                        }
                    }
                }

                //mActiveGraph->StopCreateConnection();
                mActiveGraph->StopRelinkConnection();
                mLeftMousePressed = false;
                UpdateMouseCursor(mousePos, globalPos);
                //update();
                return;
            }

            // in case we adjusted a transition start or end point
            if (mActiveGraph->GetIsRepositioningTransitionHead() || mActiveGraph->GetIsRepositioningTransitionTail())
            {
                NodeConnection* connection;
                QPoint startOffset, endOffset;
                GraphNode* sourceNode;
                GraphNode* targetNode;
                mActiveGraph->GetReplaceTransitionInfo(&connection, &startOffset, &endOffset, &sourceNode, &targetNode);

                ReplaceTransition(connection, startOffset, endOffset, sourceNode, targetNode);

                //update();
                return;
            }

            // if we are finished moving, trigger the OnMoveNode callbacks
            if (mMoveNode && mouseMoved)
            {
                OnMoveStart();

                const uint32 numNodes = mActiveGraph->GetNumNodes();
                for (uint32 n = 0; n < numNodes; ++n)
                {
                    GraphNode* currentNode = mActiveGraph->GetNode(n);
                    if (currentNode->GetIsSelected() || currentNode == mMoveNode)
                    {
                        if (mouseMoved) // prevent triggering moving when we didn't move it
                        {
                            OnMoveNode(currentNode, currentNode->GetRect().topLeft().x(), currentNode->GetRect().topLeft().y());
                        }
                    }
                }

                OnMoveEnd();
            }

            mPanning = false;
            mMoveNode = nullptr;
            UpdateMouseCursor(mousePos, globalPos);

            // get the node we click on
            node = UpdateMouseCursor(mousePos, globalPos);

            if (mRectSelecting)
            {
                // calc the selection rect
                QRect selectRect;
                CalcSelectRect(selectRect);

                // select things inside it
                if (selectRect.isEmpty() == false && mActiveGraph)
                {
                    selectRect = mActiveGraph->GetTransform().inverted().mapRect(selectRect);

                    // select nodes when alt is not pressed
                    if (mAltPressed == false)
                    {
                        const bool overwriteSelection = (mControlPressed == false);
                        mActiveGraph->SelectNodesInRect(selectRect, overwriteSelection, true, mControlPressed);
                    }
                    else // zoom into the selected rect
                    {
                        mActiveGraph->ZoomOnRect(selectRect, geometry().width(), geometry().height(), true);
                    }
                }
            }

            mLeftMousePressed   = false;
            mRectSelecting      = false;
        }

        // redraw the viewport
        //UpdateMouseCursor( mousePos, globalPos );
        //update();
    }


    // double click
    void NodeGraphWidget::mouseDoubleClickEvent(QMouseEvent* event)
    {
        // only do things when a graph is active
        if (mActiveGraph == nullptr)
        {
            return;
        }

        GetMainWindow()->DisableUndoRedo();

        // get the mouse position, calculate the global mouse position and update the relevant data
        QPoint mousePos     = event->pos();
        QPoint globalPos    = LocalToGlobal(mousePos);
        SetMousePos(globalPos);

        // left mouse button double clicked
        if (event->button() == Qt::LeftButton)
        {
            // check if double clicked on a node
            GraphNode* node = mActiveGraph->FindNode(mousePos);
            if (node == nullptr)
            {
                // if we didn't double click on a node zoom in to the clicked area
                mActiveGraph->ScrollTo(-LocalToGlobal(mousePos) + geometry().center());
                mActiveGraph->ZoomIn();
            }
        }

        // right mouse button double clicked
        if (event->button() == Qt::RightButton)
        {
            // check if double clicked on a node
            GraphNode* node = mActiveGraph->FindNode(mousePos);
            if (node == nullptr)
            {
                mActiveGraph->ScrollTo(-LocalToGlobal(mousePos) + geometry().center());
                mActiveGraph->ZoomOut();
            }
        }

        setCursor(Qt::ArrowCursor);

        // reset flags
        mRectSelecting = false;

        // redraw the viewport
        //update();
    }


    // when the mouse wheel changes
    void NodeGraphWidget::wheelEvent(QWheelEvent* event)
    {
        // only do things when a graph is active
        if (mActiveGraph == nullptr)
        {
            return;
        }

        // get the mouse position, calculate the global mouse position and update the relevant data
        
        // For some reason this call fails to get the correct mouse position
        // It might be relative to the window?  Jira generated.
//        QPoint mousePos     = event->pos();

        QPoint globalQtMousePos = event->globalPos();
        QPoint globalQtMousePosInWidget = this->mapFromGlobal(globalQtMousePos);
        QPoint globalPos    = LocalToGlobal(globalQtMousePosInWidget);

        SetMousePos(globalPos);

        // only handle vertical events
        if (event->orientation() != Qt::Vertical)
        {
            return;
        }

        // stop the automated zoom
        mActiveGraph->StopAnimatedZoom();

        // calculate the new scale value
        const float scaleDelta = (event->delta() / 120.0f) * 0.05f;
        float newScale = MCore::Clamp<float>(mActiveGraph->GetScale() + scaleDelta, mActiveGraph->GetLowestScale(), 1.0f);
        mActiveGraph->SetScale(newScale);

        // redraw the viewport
        //update();
    }


    // update the mouse cursor, based on if we hover over a given node or not
    GraphNode* NodeGraphWidget::UpdateMouseCursor(const QPoint& localMousePos, const QPoint& globalMousePos)
    {
        // if there is no active graph
        if (mActiveGraph == nullptr)
        {
            setCursor(Qt::ArrowCursor);
            return nullptr;
        }

        if (mPanning || mMoveNode)
        {
            setCursor(Qt::ClosedHandCursor);
            return nullptr;
        }

        // check if we hover above a node
        GraphNode* node = mActiveGraph->FindNode(localMousePos);

        // check if the node is valid
        // we test firstly the node to have the visualize cursor correct
        if (node)
        {
            // cast the node
            BlendTreeVisualNode* blendNode = static_cast<BlendTreeVisualNode*>(node);
            EMotionFX::AnimGraphNode* animGraphNode = blendNode->GetEMFXNode();

            // check if the node is part of a state machine, on this case it's not collapsible, the arrow is not needed to be checked
            if (azrtti_typeid(animGraphNode->GetParentNode()) == azrtti_typeid<EMotionFX::AnimGraphStateMachine>())
            {
                // if the mouse is over a node but NOT over the visualize icon
                if (node->GetIsInsideVisualizeRect(globalMousePos))
                {
                    setCursor(Qt::ArrowCursor);
                    return node;
                }
            }
            else
            {
                // if the mouse is over a node but NOT over the arrow rect or the visualize icon
                if (node->GetIsInsideArrowRect(globalMousePos) || (node->GetCanVisualize() && node->GetIsInsideVisualizeRect(globalMousePos)))
                {
                    setCursor(Qt::ArrowCursor);
                    return node;
                }
            }

            // check if we're hovering over a port
            uint32      portNr;
            GraphNode*  portNode;
            bool        isInputPort;
            NodePort* nodePort = mActiveGraph->FindPort(globalMousePos.x(), globalMousePos.y(), &portNode, &portNr, &isInputPort);
            if (nodePort)
            {
                if ((isInputPort && portNode->GetCreateConFromOutputOnly() == false) || isInputPort == false)
                {
                    setCursor(Qt::PointingHandCursor);
                    return nullptr;
                }
            }

            // set the hand cursor if we are only hovering a node
            setCursor(Qt::OpenHandCursor);
            return node;
        }
        else // not hovering a node, simply check for ports
        {
            // check if we're hovering over a port
            uint32      portNr;
            GraphNode*  portNode;
            bool        isInputPort;
            NodePort* nodePort = mActiveGraph->FindPort(globalMousePos.x(), globalMousePos.y(), &portNode, &portNr, &isInputPort);
            if (nodePort)
            {
                if ((isInputPort && portNode->GetCreateConFromOutputOnly() == false) || isInputPort == false)
                {
                    setCursor(Qt::PointingHandCursor);
                    return nullptr;
                }
            }
        }

        // the default cursor is the arrow
        setCursor(Qt::ArrowCursor);
        return node;
    }


    // resize
    void NodeGraphWidget::resizeEvent(QResizeEvent* event)
    {
        QOpenGLWidget::resizeEvent(event);
    }


    // calculate the selection rect
    void NodeGraphWidget::CalcSelectRect(QRect& outRect)
    {
        const int32 startX = MCore::Min<int32>(mSelectStart.x(), mSelectEnd.x());
        const int32 startY = MCore::Min<int32>(mSelectStart.y(), mSelectEnd.y());
        const int32 width  = abs(mSelectEnd.x() - mSelectStart.x());
        const int32 height = abs(mSelectEnd.y() - mSelectStart.y());

        outRect = QRect(startX, startY, width, height);
    }


    // on keypress
    void NodeGraphWidget::keyPressEvent(QKeyEvent* event)
    {
        switch (event->key())
        {
        case Qt::Key_Shift:
        {
            mShiftPressed     = true;
            break;
        }
        case Qt::Key_Control:
        {
            mControlPressed   = true;
            break;
        }
        case Qt::Key_Alt:
        {
            mAltPressed       = true;
            break;
        }
        }

        MysticQt::KeyboardShortcutManager* shortcutManager = GetMainWindow()->GetShortcutManager();

        if (shortcutManager->Check(event, "Fit Entire Graph", "Anim Graph Window"))
        {
            // zoom to fit the entire graph in view
            if (mActiveGraph)
            {
                mActiveGraph->FitGraphOnScreen(geometry().width(), geometry().height(), GetMousePos());
            }

            event->accept();
            return;
        }

        if (shortcutManager->Check(event, "Zoom On Selected Nodes", "Anim Graph Window"))
        {
            if (mActiveGraph)
            {
                // try zooming on the selection rect
                QRect selectionRect = mActiveGraph->CalcRectFromSelection(true);
                if (selectionRect.isEmpty() == false)
                {
                    mActiveGraph->ZoomOnRect(selectionRect, geometry().width(), geometry().height());
                    //update();
                }
                else // zoom on the full scene
                {
                    mActiveGraph->FitGraphOnScreen(geometry().width(), geometry().height(), GetMousePos());
                }
            }

            event->accept();
            return;
        }

        event->ignore();
    }


    // on key release
    void NodeGraphWidget::keyReleaseEvent(QKeyEvent* event)
    {
        switch (event->key())
        {
        case Qt::Key_Shift:
        {
            mShiftPressed     = false;
            break;
        }
        case Qt::Key_Control:
        {
            mControlPressed   = false;
            break;
        }
        case Qt::Key_Alt:
        {
            mAltPressed       = false;
            break;
        }
        }

        MysticQt::KeyboardShortcutManager* shortcutManager = GetMainWindow()->GetShortcutManager();

        if (shortcutManager->Check(event, "Fit Entire Graph", "Anim Graph Window"))
        {
            event->accept();
            return;
        }

        if (shortcutManager->Check(event, "Zoom On Selected Nodes", "Anim Graph Window"))
        {
            event->accept();
            return;
        }

        event->ignore();
    }


    // receiving focus
    void NodeGraphWidget::focusInEvent(QFocusEvent* event)
    {
        MCORE_UNUSED(event);
        grabKeyboard();
        //update();
    }


    // out of focus
    void NodeGraphWidget::focusOutEvent(QFocusEvent* event)
    {
        MCORE_UNUSED(event);
        mShiftPressed   = false;
        mControlPressed = false;
        mAltPressed     = false;
        releaseKeyboard();

        if (mActiveGraph && mActiveGraph->GetIsCreatingConnection())
        {
            mActiveGraph->StopCreateConnection();
            mLeftMousePressed = false;
        }
        //update();
    }


    // return the number of selected nodes
    uint32 NodeGraphWidget::CalcNumSelectedNodes() const
    {
        if (mActiveGraph)
        {
            return mActiveGraph->CalcNumSelectedNodes();
        }
        else
        {
            return 0;
        }
    }


    // is the given connection valid
    bool NodeGraphWidget::CheckIfIsCreateConnectionValid(uint32 portNr, GraphNode* portNode, NodePort* port, bool isInputPort)
    {
        MCORE_UNUSED(portNr);
        MCORE_UNUSED(port);

        if (mActiveGraph == nullptr)
        {
            return false;
        }

        GraphNode* sourceNode = mActiveGraph->GetCreateConnectionNode();
        GraphNode* targetNode = portNode;

        // don't allow connection to itself
        if (sourceNode == targetNode)
        {
            return false;
        }

        // dont allow to connect an input port to another input port or output port to another output port
        if (isInputPort == mActiveGraph->GetCreateConnectionIsInputPort())
        {
            return false;
        }

        return true;
    }

    bool NodeGraphWidget::CheckIfIsValidTransition(GraphNode* sourceState, GraphNode* targetState)
    {
        AZ_UNUSED(sourceState);
        AZ_UNUSED(targetState);

        return true;
    }

    bool NodeGraphWidget::CheckIfIsValidTransitionSource(GraphNode* sourceState)
    {
        AZ_UNUSED(sourceState);

        return true;
    }

    // create the connection
    void NodeGraphWidget::OnCreateConnection(uint32 sourcePortNr, GraphNode* sourceNode, bool sourceIsInputPort, uint32 targetPortNr, GraphNode* targetNode, bool targetIsInputPort, const QPoint& startOffset, const QPoint& endOffset)
    {
        MCORE_UNUSED(targetIsInputPort);
        MCORE_UNUSED(startOffset);
        MCORE_UNUSED(endOffset);

        //MCore::LogDebug("Creating connection");
        if (mActiveGraph == nullptr)
        {
            return;
        }

        // create the connection if it doesn't exist already
        if (sourceIsInputPort == false)
        {
            if (mActiveGraph->CheckIfHasConnection(sourceNode, sourcePortNr, targetNode, targetPortNr) == false)
            {
                mActiveGraph->AddConnection(sourceNode, sourcePortNr, targetNode, targetPortNr);
            }
        }
        else
        {
            if (mActiveGraph->CheckIfHasConnection(targetNode, targetPortNr, sourceNode, sourcePortNr) == false)
            {
                mActiveGraph->AddConnection(targetNode, targetPortNr, sourceNode, sourcePortNr);
            }
        }
    }


    void NodeGraphWidget::SetCallback(GraphWidgetCallback* callback)
    {
        delete mCallback;
        mCallback = callback;
    }
} // namespace EMStudio

#include <EMotionFX/Tools/EMotionStudio/Plugins/StandardPlugins/Source/AnimGraph/NodeGraphWidget.moc>
