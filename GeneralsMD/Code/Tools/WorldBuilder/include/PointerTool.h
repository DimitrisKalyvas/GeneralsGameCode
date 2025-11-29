/*
**	Command & Conquer Generals Zero Hour(tm)
**	Copyright 2025 Electronic Arts Inc.
**
**	This program is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 3 of the License, or
**	(at your option) any later version.
**
**	This program is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// PointerTool.h
// Texture tiling tools for worldbuilder.
// Author: John Ahlquist, April 2001

#pragma once

#include "PolygonTool.h"
class WorldHeightMapEdit;
#include "../../GameEngine/Include/Common/MapObject.h"

class ModifyObjectUndoable;

enum GizmoComponent {
	GIZMO_NONE = 0,       ///< No gizmo component
	GIZMO_MOVE_X,         ///< X-axis translation handle (red)
	GIZMO_MOVE_Y,         ///< Y-axis translation handle (green)
	GIZMO_MOVE_Z,         ///< Z-axis translation handle (blue)
	GIZMO_MOVE_XY,        ///< XY plane translation handle
	GIZMO_ROTATE_Z,       ///< Z-axis rotation ring (yaw)
};

enum GizmoMode {
	GIZMO_MODE_TRANSLATE,
	GIZMO_MODE_ROTATE,
};

/*************************************************************************/
/**                             PointerTool
	 Does the select/move tool operation.
***************************************************************************/
///  Blend edges out tool.
class PointerTool : public PolygonTool
{
protected:
	enum {HYSTERESIS = 3};
	CPoint m_downPt2d;
	Coord3D m_downPt3d;
	MapObject *m_curObject;

	Bool m_moving; ///< True if we are drag moving an object.
	Bool m_rotating; ///< True if we are rotating an object.
	Bool m_dragSelect; ///< True if we are drag selecting.

	Bool m_doPolyTool; ///< True if we are using the polygon tool to modify a polygon triggter.

	ModifyObjectUndoable *m_modifyUndoable;	 ///< The modify undoable that is in progress while we track the mouse.

	Bool m_mouseUpRotate;///< True if we are over the "rotate" hotspot.
	HCURSOR m_rotateCursor;
	Bool m_mouseUpMove;///< True if we are over the "move" hotspot.
	HCURSOR m_moveCursor;

	GizmoMode m_gizmoMode;
	GizmoComponent m_hoveredGizmoComponent;
	GizmoComponent m_activeGizmoComponent;
	Coord3D m_gizmoCenter;
	Real m_gizmoScale;
	Real m_gizmoAngle;
	Coord3D m_gizmoDragStartPos;
	Real m_gizmoDragStartAngle;
	Real m_gizmoRotationDelta;
	Real m_gizmoStartAngleForDisplay;
	Bool m_gizmoVisible;
	Bool m_gizmoRotating;

protected:
	void checkForPropertiesPanel(void);
	
	void updateGizmoCenter(void);
	void updateGizmoScale(WbView* pView);
	GizmoComponent pickGizmoComponent(CPoint viewPt, WbView* pView);
	void handleGizmoTranslation(GizmoComponent axis, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc);
	void handleGizmoRotation(CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc);

public:
	PointerTool(void);
	~PointerTool(void);

public:
	/// Clear the selection on activate or deactivate.
	virtual void activate();
	virtual void deactivate();

	virtual void setCursor(void);
	virtual void mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);
	virtual void mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc);

public:
	static void clearSelection(void); ///< Clears the selected objects selected flags.
	static Bool allowPick(MapObject* pMapObj, WbView* pView);
	
	Bool isGizmoVisible(void) const { return m_gizmoVisible; }
	const Coord3D& getGizmoCenter(void) const { return m_gizmoCenter; }
	Real getGizmoScale(void) const { return m_gizmoScale; }
	Real getGizmoAngle(void) const { return m_gizmoAngle; }
	GizmoComponent getHoveredGizmoComponent(void) const { return m_hoveredGizmoComponent; }
	GizmoMode getGizmoMode(void) const { return m_gizmoMode; }
	Bool isGizmoRotating(void) const { return m_gizmoRotating; }
	Real getGizmoRotationDelta(void) const { return m_gizmoRotationDelta; }
	Real getGizmoStartAngle(void) const { return m_gizmoStartAngleForDisplay; }
	void setGizmoMode(GizmoMode mode);
};
