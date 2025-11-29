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

// PointerTool.cpp
// Texture tiling tool for worldbuilder.
// Author: John Ahlquist, April 2001

#include "StdAfx.h"
#include "resource.h"

#include "PointerTool.h"
#include "CUndoable.h"
#include "MainFrm.h"
#include "WHeightMapEdit.h"
#include "WorldBuilderDoc.h"
#include "WorldBuilderView.h"
#include "GameLogic/SidesList.h"
#include "Common/ThingSort.h"
#include "Common/ThingTemplate.h"
#include "GameLogic/PolygonTrigger.h"
#include "wbview3d.h"
#include "ObjectTool.h"
#include "W3DDevice/GameClient/HeightMap.h"

// Static member initialization
Real PointerTool::s_rotationSnapDegrees = 3.0f;
Bool PointerTool::s_lockZAxis = true;

//
// Static helper functions
// This function spiders out and un/picks all Waypoints that have some form of indirect contact with this point
// Has a recursive helper function as well.
//
static void helper_pickAllWaypointsInPath( Int sourceID, CWorldBuilderDoc *pDoc, const Int numWaypointLinks, std::vector<Int>& alreadyTouched );

/// Helper to invalidate all selected map objects in view
static void invalSelectedObjectsInView(WbView* pView)
{
	for (MapObject *obj = MapObject::getFirstMapObject(); obj; obj = obj->getNext()) {
		if (obj->isSelected()) {
			pView->invalObjectInView(obj);
		}
	}
}

static void pickAllWaypointsInPath( Int sourceID, Bool select )
{
	std::vector<Int> alreadyTouched;
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();

	helper_pickAllWaypointsInPath(sourceID, pDoc, pDoc->getNumWaypointLinks(), alreadyTouched);

	// already touched should now be filled with waypointIDs that want to be un/selected
	MapObject *pMapObj = MapObject::getFirstMapObject();
	while (pMapObj) {
		if (pMapObj->isWaypoint()) {
			if (std::find(alreadyTouched.begin(), alreadyTouched.end(), pMapObj->getWaypointID()) != alreadyTouched.end()) {
				pMapObj->setSelected(select);
			}
		}
		pMapObj = pMapObj->getNext();
	}
}

static void helper_pickAllWaypointsInPath( Int sourceID, CWorldBuilderDoc *pDoc, const Int numWaypointLinks, std::vector<Int>& alreadyTouched )
{
	if (std::find(alreadyTouched.begin(), alreadyTouched.end(), sourceID) != alreadyTouched.end() ) {
		return;
	}

	alreadyTouched.push_back(sourceID);
	for (int i = 0; i < numWaypointLinks; ++i) {
		Int way1, way2;
		pDoc->getWaypointLink(i, &way1, &way2);
		if (way1 == sourceID) {
			helper_pickAllWaypointsInPath(way2, pDoc, numWaypointLinks, alreadyTouched);
		}

		if (way2 == sourceID) {
			helper_pickAllWaypointsInPath(way1, pDoc, numWaypointLinks, alreadyTouched);
		}
	}
}

//
// PointerTool class.
//

/// Constructor
PointerTool::PointerTool(void) :
	m_modifyUndoable(NULL),
	m_curObject(NULL),
	m_rotateCursor(NULL),
	m_moveCursor(NULL),
	m_gizmoMode(GIZMO_MODE_TRANSLATE),
	m_hoveredGizmoComponent(GIZMO_NONE),
	m_activeGizmoComponent(GIZMO_NONE),
	m_gizmoCenter({0, 0, 0}),
	m_gizmoScale(1.0f),
	m_gizmoAngle(0),
	m_gizmoDragStartPos({0, 0, 0}),
	m_gizmoDragStartAngle(0),
	m_gizmoDragStartZ(0),
	m_gizmoPrevMouseAngle(0),
	m_gizmoAccumulatedDelta(0),
	m_gizmoRotationDelta(0),
	m_gizmoStartAngleForDisplay(0),
	m_gizmoObjectStartAngle(0),
	m_gizmoVisible(false),
	m_gizmoRotating(false),
	m_gizmoIsPolygon(false),
	m_gizmoPolygonStartCenter({0, 0, 0})
{
	m_toolID = ID_POINTER_TOOL;
	m_cursorID = IDC_POINTER;
}

/// Destructor
PointerTool::~PointerTool(void)
{
	REF_PTR_RELEASE(m_modifyUndoable);
	if (m_rotateCursor) ::DestroyCursor(m_rotateCursor);
	if (m_moveCursor) ::DestroyCursor(m_moveCursor);
}

/// See if a single obj is selected that has properties.
void PointerTool::checkForPropertiesPanel(void)
{
	MapObject *theMapObj = WaypointOptions::getSingleSelectedWaypoint();
	PolygonTrigger *theTrigger = WaypointOptions::getSingleSelectedPolygon();
	MapObject *theLightObj = LightOptions::getSingleSelectedLight();
	MapObject *theObj = MapObjectProps::getSingleSelectedMapObject();
	if (theMapObj) {
		CMainFrame::GetMainFrame()->showOptionsDialog(IDD_WAYPOINT_OPTIONS);
		WaypointOptions::update();
	} else if (theTrigger) {
		if (theTrigger->isWaterArea()) {
			CMainFrame::GetMainFrame()->showOptionsDialog(IDD_WATER_OPTIONS);
			WaterOptions::update();
		} else {
			CMainFrame::GetMainFrame()->showOptionsDialog(IDD_WAYPOINT_OPTIONS);
			WaypointOptions::update();
		}
	} else if (theLightObj) {
		CMainFrame::GetMainFrame()->showOptionsDialog(IDD_LIGHT_OPTIONS);
		LightOptions::update();
	} else if (RoadOptions::selectionIsRoadsOnly()) {
		CMainFrame::GetMainFrame()->showOptionsDialog(IDD_ROAD_OPTIONS);
		RoadOptions::updateSelection();
	} else {
		CMainFrame::GetMainFrame()->showOptionsDialog(IDD_MAPOBJECT_PROPS);
		MapObjectProps::update();
		if (theObj) {
			ObjectOptions::selectObject(theObj);
		}
	}
}

/// Clear the selection..
void PointerTool::clearSelection(void)
{
	// Clear map object selection
	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		pObj->setSelected(false);
	}
	
	// Clear build list selection
	for (Int i = 0; i < TheSidesList->getNumSides(); i++) {
		SidesInfo *pSide = TheSidesList->getSideInfo(i);
		for (BuildListInfo *pBuild = pSide->getBuildList(); pBuild; pBuild = pBuild->getNext()) {
			pBuild->setSelected(false);
		}
	}
	
	m_poly_curSelectedPolygon = NULL;
	
	// Update gizmo visibility
	PointerTool* pointerTool = WbApp()->getPointerTool();
	if (pointerTool) {
		pointerTool->refreshGizmo();
	}
}

/// Activate.
void PointerTool::activate()
{
	Tool::activate();
	m_hoveredGizmoComponent = GIZMO_NONE;
	m_activeGizmoComponent = GIZMO_NONE;
	checkForPropertiesPanel();
	updateGizmoCenter();  // Update gizmo visibility based on selection
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc==NULL) return;
	WbView3d *p3View = pDoc->GetActive3DView();
	p3View->setObjTracking(NULL, m_downPt3d, 0, false);
}

/// deactivate.
void PointerTool::deactivate()
{
	m_curObject = NULL;
	m_gizmoVisible = false;
	PolygonTool::deactivate();
}

/** Set the cursor. */
void PointerTool::setCursor(void)
{
	if (m_hoveredGizmoComponent == GIZMO_ROTATE_Z) {
		if (!m_rotateCursor) {
			m_rotateCursor = AfxGetApp()->LoadCursor(MAKEINTRESOURCE(IDC_ROTATE));
		}
		::SetCursor(m_rotateCursor);
		return;
	}
	if (m_hoveredGizmoComponent != GIZMO_NONE) {
		if (!m_moveCursor) {
			m_moveCursor = AfxGetApp()->LoadCursor(MAKEINTRESOURCE(IDC_MOVE_POINTER));
		}
		::SetCursor(m_moveCursor);
		return;
	}
	Tool::setCursor();
}

Bool PointerTool::allowPick(MapObject* pMapObj, WbView* pView)
{
	EditorSortingType sort = ES_NONE;
	if (!pMapObj) {
		return false;
	}
	const ThingTemplate *tt = pMapObj->getThingTemplate();
	if (tt && tt->getEditorSorting() == ES_AUDIO) {
		if (pView->GetPickConstraint() == ES_NONE || pView->GetPickConstraint() == ES_AUDIO) {
			return true;
		}
	}
	if ((tt && !pView->getShowModels()) || (pMapObj->getFlags() & FLAG_DONT_RENDER)) {
		return false;
	}
	if (pView->GetPickConstraint() != ES_NONE) {
		if (tt) {
			if (!pView->getShowModels()) {
				return false;
			}
			sort = tt->getEditorSorting();
		} else {
			if (pMapObj->isWaypoint()) {
				sort = ES_WAYPOINT;
			}
			if (pMapObj->getFlag(FLAG_ROAD_FLAGS)) {
				sort = ES_ROAD;
			}
		}
		if (sort != ES_NONE && sort != pView->GetPickConstraint()) {
			return false;
		}
	}
	return true;
}

/** Execute the tool on mouse down - Pick an object. */
void PointerTool::mouseDown(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) return;

	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);
	Coord3D loc;

	m_downPt2d = viewPt;
	m_downPt3d = cpt;
	pView->snapPoint(&m_downPt3d);
	m_moving = false;
	m_dragSelect = false;
	Bool shiftKey = (0x8000 & ::GetAsyncKeyState(VK_SHIFT))!=0;
	Bool ctrlKey = (0x8000 & ::GetAsyncKeyState(VK_CONTROL))!=0;

	updateGizmoScale(pView);
	GizmoComponent gizmoHit = pickGizmoComponent(viewPt, pView);
	if (gizmoHit != GIZMO_NONE) {
		m_activeGizmoComponent = gizmoHit;
		m_gizmoDragStartPos = cpt;
		m_gizmoPolygonStartCenter = m_gizmoCenter;
		
		if (m_gizmoIsPolygon) {
			PolygonTool::storePolygonStartPositions();
		}
		
		if (gizmoHit == GIZMO_ROTATE_Z) {
			Real dx = cpt.x - m_gizmoCenter.x;
			Real dy = cpt.y - m_gizmoCenter.y;
			m_gizmoDragStartAngle = atan2(dy, dx);
			m_gizmoPrevMouseAngle = m_gizmoDragStartAngle;
			m_gizmoAccumulatedDelta = 0;
			m_gizmoStartAngleForDisplay = m_gizmoAngle;
			m_gizmoObjectStartAngle = m_gizmoAngle;
			m_gizmoRotationDelta = 0;
			m_gizmoRotating = true;
		}
		
		if (gizmoHit == GIZMO_MOVE_Z) {
			m_gizmoDragStartZ = m_gizmoCenter.z;
		}
		
		if (m_modifyUndoable == NULL) {
			m_modifyUndoable = new ModifyObjectUndoable(pDoc);
		}
		
		m_moving = true;
		
		MapObject *pObj = MapObject::getFirstMapObject();
		while (pObj) {
			if (pObj->isSelected()) {
				m_curObject = pObj;
				break;
			}
			pObj = pObj->getNext();
		}
		return;
	}

	m_doPolyTool = false;
	if (pView->GetPickConstraint() == ES_NONE || pView->GetPickConstraint() == ES_WAYPOINT) {
		// If polygon triggers are visible, see if we clicked on one.
		if (pView->isPolygonTriggerVisible()) {
			m_poly_unsnappedMouseDownPt = cpt;
			poly_pickOnMouseDown(viewPt, pView);
			if (m_poly_curSelectedPolygon) {
				// picked on one.
				if (!poly_snapToPoly(&cpt)) {
					pView->snapPoint(&cpt);
				}
				m_poly_mouseDownPt = cpt;
				m_poly_justPicked = true; // Makes poly tool move instead of inserting.
				m_doPolyTool = true;
				PolygonTool::startMouseDown(m, viewPt, pView, pDoc);
				return;
			}
			m_poly_curSelectedPolygon = NULL;
			m_poly_dragPointNdx = -1;
		}
	}



//	WorldHeightMapEdit *pMap = pDoc->GetHeightMap();
	m_curObject = NULL;
	MapObject *pObj = MapObject::getFirstMapObject();
	MapObject *p3DObj = pView->picked3dObjectInView(viewPt);
	MapObject *pClosestPicked = NULL;
	if (allowPick(p3DObj, pView)) {
		pClosestPicked = p3DObj;
	}
	Real pickDistSqr = 10000*MAP_XY_FACTOR;
	pickDistSqr *= pickDistSqr;

	// Find the closest pick.
	for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (!allowPick(pObj, pView)) {
			continue;
		}
		Bool picked = (pView->picked(pObj, cpt) != PICK_NONE);
		if (picked) {
			loc = *pObj->getLocation();
			Real dx = m_downPt3d.x-loc.x;
			Real dy = m_downPt3d.y-loc.y;
			Real distSqr = dx*dx+dy*dy;
			if (distSqr < pickDistSqr) {
				pClosestPicked = pObj;
				pickDistSqr = distSqr;
			}
		}
	}

	Bool anySelected = (pClosestPicked!=NULL);
	if (shiftKey) {
		if (pClosestPicked && pClosestPicked->isSelected()) {
			pClosestPicked->setSelected(false);
			if (ctrlKey && pClosestPicked->isWaypoint()) {
				pickAllWaypointsInPath(pClosestPicked->getWaypointID(), false);
			}
		} else if (pClosestPicked) {
			pClosestPicked->setSelected(true);
			if (ctrlKey && pClosestPicked->isWaypoint()) {
				pickAllWaypointsInPath(pClosestPicked->getWaypointID(), true);
			}
		}
	} else if (pClosestPicked && pClosestPicked->isSelected()) {
		// We picked a selected object
			m_curObject = pClosestPicked;
	} else {
		clearSelection();
		if (pClosestPicked) {
			pClosestPicked->setSelected(true);
			if (ctrlKey && pClosestPicked->isWaypoint()) {
				pickAllWaypointsInPath(pClosestPicked->getWaypointID(), true);
			}

		}
	}

	// Grab both ends of a road.
	if (pView->GetPickConstraint() == ES_NONE || pView->GetPickConstraint() == ES_ROAD) {
		if (!shiftKey && pClosestPicked && (pClosestPicked->getFlags()&FLAG_ROAD_FLAGS) ) {
			for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
				if (pObj->getFlags()&FLAG_ROAD_FLAGS) {
					loc = *pObj->getLocation();
					Real dx = pClosestPicked->getLocation()->x - loc.x;
					Real dy = pClosestPicked->getLocation()->y - loc.y;
					Real dist = sqrt(dx*dx+dy*dy);
					if (dist < MAP_XY_FACTOR/100) {
						pObj->setSelected(true);
					}
				}
			}
		}
	}

	if (anySelected) {
		if (!m_curObject) {
			pObj = MapObject::getFirstMapObject();
			while (pObj) {
				if (pObj->isSelected()) {
					m_curObject = pObj;
					break;
				}
				pObj = pObj->getNext();
			}
		}
		if (m_curObject && !m_gizmoVisible) {
			loc = *m_curObject->getLocation();
			Coord3D snapLoc = loc;
			pView->snapPoint(&snapLoc);
			m_downPt3d.x += (loc.x-snapLoc.x);
			m_downPt3d.y += (loc.y-snapLoc.y);
		}
	}	else {
		m_dragSelect = true;
	}
	
	// Update gizmo after selection changes
	updateGizmoCenter();
}

/// Left button move code.
void PointerTool::mouseMoved(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt, false);
	
	// Update gizmo scale
	updateGizmoScale(pView);
	
	if (m == TRACK_NONE) {
		// Update hovered gizmo component
		GizmoComponent prevHovered = m_hoveredGizmoComponent;
		m_hoveredGizmoComponent = pickGizmoComponent(viewPt, pView);
		
		// Invalidate view if hover state changed
		if (prevHovered != m_hoveredGizmoComponent) {
			CWorldBuilderDoc *pDocLocal = CWorldBuilderDoc::GetActiveDoc();
			if (pDocLocal) {
				WbView3d *p3View = pDocLocal->Get3DView();
				if (p3View) {
					p3View->Invalidate(false);
				}
			}
		}
		return;
	}

	if (m != TRACK_L) return;
	if (m_doPolyTool) {
		PolygonTool::mouseMoved(m, viewPt, pView, pDoc);
		return;
	}

	if (m_dragSelect) {
		CRect box;
		box.left = viewPt.x;
		box.bottom = viewPt.y;
		box.top = m_downPt2d.y;
		box.right = m_downPt2d.x;
		box.NormalizeRect();
		pView->doRectFeedback(true, box);
		pView->Invalidate();
		return;
	}

	// Handle gizmo dragging
	if (m_activeGizmoComponent != GIZMO_NONE && m_moving) {
		if (m_gizmoIsPolygon) {
			// Polygon gizmo - lightweight path
			if (m_activeGizmoComponent == GIZMO_ROTATE_Z) {
				handleGizmoRotation(viewPt, pView, pDoc);
			} else {
				handleGizmoTranslation(m_activeGizmoComponent, viewPt, pView, pDoc);
			}
			pView->Invalidate();
			pView->UpdateWindow();
			return;
		}
		
		// MapObject gizmo - requires undoable
		if (!m_modifyUndoable) return;
		
		invalSelectedObjectsInView(pView);
		
		if (m_activeGizmoComponent == GIZMO_ROTATE_Z) {
			handleGizmoRotation(viewPt, pView, pDoc);
		} else {
			handleGizmoTranslation(m_activeGizmoComponent, viewPt, pView, pDoc);
		}
		
		invalSelectedObjectsInView(pView);
		updateGizmoCenter();
		pDoc->updateAllViews();
		return;
	}

	if (m_curObject == NULL) {
		return;
	}
	pView->viewToDocCoords(viewPt, &cpt, true);
	if (!m_moving) {
		// always use view coords (not doc coords) for hysteresis
		Int dx = viewPt.x-m_downPt2d.x;
		Int dy = viewPt.y-m_downPt2d.y;
		if (abs(dx)>HYSTERESIS || abs(dy)>HYSTERESIS) {
			m_moving = true;
			m_modifyUndoable = new ModifyObjectUndoable(pDoc);
		}
	}
	if (!m_moving || !m_modifyUndoable) return;

	invalSelectedObjectsInView(pView);

	pView->snapPoint(&cpt);
	Real xOffset = (cpt.x-m_downPt3d.x);
	Real yOffset = (cpt.y-m_downPt3d.y);
	m_modifyUndoable->SetOffset(xOffset, yOffset);

	invalSelectedObjectsInView(pView);
	updateGizmoCenter();
	pDoc->updateAllViews();

}


/** Execute the tool on mouse up - if modifying, do the modify,
else update the selection. */
void PointerTool::mouseUp(TTrackingMode m, CPoint viewPt, WbView* pView, CWorldBuilderDoc *pDoc)
{
	if (m != TRACK_L) return;

	if (m_doPolyTool) {
		m_doPolyTool = false;
		PolygonTool::mouseUp(m, viewPt, pView, pDoc);
		checkForPropertiesPanel();
		return;
	}

	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);

	if (m_activeGizmoComponent != GIZMO_NONE && m_moving && m_modifyUndoable) {
		pDoc->AddAndDoUndoable(m_modifyUndoable);
		REF_PTR_RELEASE(m_modifyUndoable);
		
		m_activeGizmoComponent = GIZMO_NONE;
		m_moving = false;
		m_gizmoRotating = false;
		m_gizmoRotationDelta = 0;
		
		updateGizmoCenter();
		checkForPropertiesPanel();
		return;
	}

	if (m_curObject && m_moving) {
		pDoc->AddAndDoUndoable(m_modifyUndoable);
		REF_PTR_RELEASE(m_modifyUndoable); // belongs to pDoc now.
	}	else if (m_dragSelect) {
		CRect box;
		box.left = viewPt.x;
		box.top = viewPt.y;
		box.right = m_downPt2d.x;
		box.bottom = m_downPt2d.y;
		box.NormalizeRect();
		pView->doRectFeedback(false, box);
		pView->Invalidate();

		MapObject *pObj;
		for (pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
			// Don't pick on invisible waypoints
			if (pObj->isWaypoint() && !pView->isWaypointVisible()) {
				continue;
			}
			if (!allowPick(pObj, pView)) {
				continue;
			}
			Bool picked;
			Coord3D loc = *pObj->getLocation();
			CPoint viewPt;
			if (pView->docToViewCoords(loc, &viewPt)){
				picked = (viewPt.x>=box.left && viewPt.x<=box.right && viewPt.y>=box.top && viewPt.y<=box.bottom) ;
				if (picked) {
					if ((0x8000 && ::GetAsyncKeyState(VK_SHIFT))) {
						pObj->setSelected(!pObj->isSelected());
					}	else {
						pObj->setSelected(true);
					}
					pDoc->invalObject(pObj);
				}
			}
		}

	}
	checkForPropertiesPanel();
	updateGizmoCenter();
}

void PointerTool::updateGizmoCenter(void)
{
	Int count = 0;
	Coord3D center = {0, 0, 0};
	Real firstAngle = 0;
	
	for (MapObject *pObj = MapObject::getFirstMapObject(); pObj; pObj = pObj->getNext()) {
		if (pObj->isSelected()) {
			const Coord3D* loc = pObj->getLocation();
			center.x += loc->x;
			center.y += loc->y;
			center.z += loc->z;
			if (count == 0) firstAngle = pObj->getAngle();
			count++;
		}
	}
	
	if (count > 0) {
		m_gizmoCenter.x = center.x / count;
		m_gizmoCenter.y = center.y / count;
		m_gizmoCenter.z = center.z / count;
		m_gizmoAngle = firstAngle;
		m_gizmoVisible = true;
		m_gizmoIsPolygon = false;
		return;
	}
	
	// Check for polygon selection
	Coord3D loc;
	if (PolygonTool::getSelectedPointLocation(&loc) || PolygonTool::getSelectedPolygonCenter(&loc)) {
		m_gizmoCenter = loc;
		m_gizmoAngle = 0;
		m_gizmoVisible = true;
		m_gizmoIsPolygon = true;
	} else {
		m_gizmoVisible = false;
		m_gizmoIsPolygon = false;
	}
}

void PointerTool::updateGizmoScale(WbView* pView)
{
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) {
		WbView3d *p3View = pDoc->Get3DView();
		if (p3View) {
			Real zoom = p3View->getCurrentZoom();
			m_gizmoScale = max(0.5f, min(3.0f, zoom / 200.0f));
		}
	}
}

static const Int GIZMO_AXIS_LENGTH = 40;
static const Int GIZMO_HANDLE_SIZE = 20;
static const Int GIZMO_RING_RADIUS = 35;
static const Int GIZMO_PICK_TOLERANCE = 18;

GizmoComponent PointerTool::pickGizmoComponent(CPoint viewPt, WbView* pView)
{
	if (!m_gizmoVisible) {
		return GIZMO_NONE;
	}
	
	CPoint centerPt;
	Coord3D gizmoWorldPos = m_gizmoCenter;
	if (!pView->docToViewCoords(gizmoWorldPos, &centerPt)) {
		return GIZMO_NONE;
	}
	
	Real scale = GIZMO_AXIS_LENGTH * m_gizmoScale;
	
	Coord3D testEnd = gizmoWorldPos;
	testEnd.x += scale;
	CPoint testPt;
	pView->docToViewCoords(testEnd, &testPt);
	Real screenAxisLen = sqrt((Real)(testPt.x - centerPt.x)*(testPt.x - centerPt.x) + 
	                          (Real)(testPt.y - centerPt.y)*(testPt.y - centerPt.y));
	Int tolerance = max(12, (Int)(screenAxisLen * 0.3f));
	
	auto distToSegment = [](CPoint p, CPoint a, CPoint b) -> Real {
		Real dx = (Real)(b.x - a.x);
		Real dy = (Real)(b.y - a.y);
		Real lengthSq = dx*dx + dy*dy;
		if (lengthSq < 0.001f) return sqrt((Real)(p.x-a.x)*(p.x-a.x) + (Real)(p.y-a.y)*(p.y-a.y));
		
		Real t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / lengthSq;
		if (t < 0) t = 0;
		if (t > 1) t = 1;
		
		Real projX = a.x + t * dx;
		Real projY = a.y + t * dy;
		return sqrt((p.x - projX)*(p.x - projX) + (p.y - projY)*(p.y - projY));
	};
	
	if (m_gizmoMode == GIZMO_MODE_ROTATE) {
		Coord3D ringEdge = m_gizmoCenter;
		ringEdge.x += GIZMO_RING_RADIUS * m_gizmoScale;
		CPoint ringPt;
		pView->docToViewCoords(ringEdge, &ringPt);
		Real screenRingRadius = sqrt((Real)(ringPt.x - centerPt.x)*(ringPt.x - centerPt.x) + 
		                             (Real)(ringPt.y - centerPt.y)*(ringPt.y - centerPt.y));
		
		Real dx = (Real)(viewPt.x - centerPt.x);
		Real dy = (Real)(viewPt.y - centerPt.y);
		Real dist = sqrt(dx*dx + dy*dy);
		
		Real ringTolerance = max((Real)tolerance, screenRingRadius * 0.35f);
		if (fabs(dist - screenRingRadius) < ringTolerance) {
			return GIZMO_ROTATE_Z;
		}
	}
	
	if (m_gizmoMode == GIZMO_MODE_TRANSLATE) {
		Real cosA = cos(m_gizmoAngle);
		Real sinA = sin(m_gizmoAngle);
		
		Coord3D xEnd = gizmoWorldPos;
		xEnd.x += scale * cosA;
		xEnd.y += scale * sinA;
		CPoint xPt;
		pView->docToViewCoords(xEnd, &xPt);
		
		Coord3D yEnd = gizmoWorldPos;
		yEnd.x += scale * (-sinA);
		yEnd.y += scale * cosA;
		CPoint yPt;
		pView->docToViewCoords(yEnd, &yPt);
		
		Coord3D zEnd = gizmoWorldPos;
		zEnd.z += scale;
		CPoint zPt;
		pView->docToViewCoords(zEnd, &zPt);
		
		if (distToSegment(viewPt, centerPt, zPt) < tolerance) return GIZMO_MOVE_Z;
		if (distToSegment(viewPt, centerPt, xPt) < tolerance) return GIZMO_MOVE_X;
		if (distToSegment(viewPt, centerPt, yPt) < tolerance) return GIZMO_MOVE_Y;
		
		Real planeSize = 20.0f * m_gizmoScale;
		Real pCos = planeSize * cosA;
		Real pSin = planeSize * sinA;
		Coord3D p1 = gizmoWorldPos; p1.x += pCos - pSin; p1.y += pSin + pCos;
		Coord3D p2 = gizmoWorldPos; p2.x += -pCos - pSin; p2.y += -pSin + pCos;
		Coord3D p3 = gizmoWorldPos; p3.x += -pCos + pSin; p3.y += -pSin - pCos;
		Coord3D p4 = gizmoWorldPos; p4.x += pCos + pSin; p4.y += pSin - pCos;
		
		CPoint sp1, sp2, sp3, sp4;
		if (pView->docToViewCoords(p1, &sp1) && pView->docToViewCoords(p2, &sp2) &&
		    pView->docToViewCoords(p3, &sp3) && pView->docToViewCoords(p4, &sp4)) {
			auto cross = [](CPoint o, CPoint a, CPoint b) -> Real {
				return (Real)(a.x - o.x) * (b.y - o.y) - (Real)(a.y - o.y) * (b.x - o.x);
			};
			Bool inside = (cross(sp1, sp2, viewPt) >= 0) == (cross(sp2, sp3, viewPt) >= 0) &&
			              (cross(sp2, sp3, viewPt) >= 0) == (cross(sp3, sp4, viewPt) >= 0) &&
			              (cross(sp3, sp4, viewPt) >= 0) == (cross(sp4, sp1, viewPt) >= 0);
			if (inside) return GIZMO_MOVE_XY;
		}
	}
	
	return GIZMO_NONE;
}

void PointerTool::handleGizmoTranslation(GizmoComponent axis, CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc)
{
	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);
	
	Real dx = cpt.x - m_gizmoDragStartPos.x;
	Real dy = cpt.y - m_gizmoDragStartPos.y;
	
	Real cosA = cos(m_gizmoAngle);
	Real sinA = sin(m_gizmoAngle);
	
	Real xOffset = 0;
	Real yOffset = 0;
	Real zOffset = 0;
	
	switch (axis) {
		case GIZMO_MOVE_X: {
			Real localX = dx * cosA + dy * sinA;
			xOffset = localX * cosA;
			yOffset = localX * sinA;
			break;
		}
			
		case GIZMO_MOVE_Y: {
			Real localY = -dx * sinA + dy * cosA;
			xOffset = localY * (-sinA);
			yOffset = localY * cosA;
			break;
		}
			
		case GIZMO_MOVE_Z:
			if (s_lockZAxis) {
				return;
			}
			{
				Real sensitivity = 0.5f;
				zOffset = m_gizmoDragStartZ + (dy * sensitivity);
			}
			break;
			
		case GIZMO_MOVE_XY:
			xOffset = dx;
			yOffset = dy;
			break;
			
		default:
			return;
	}
	
	Coord3D snapped = m_gizmoPolygonStartCenter;
	snapped.x += xOffset;
	snapped.y += yOffset;
	pView->snapPoint(&snapped);
	
	Real snappedXOffset = snapped.x - m_gizmoPolygonStartCenter.x;
	Real snappedYOffset = snapped.y - m_gizmoPolygonStartCenter.y;
	
	if (m_gizmoIsPolygon) {
		if (axis != GIZMO_MOVE_Z) {
			if (PolygonTool::hasSelectedPoint()) {
				PolygonTool::setSelectedPointOffset(snappedXOffset, snappedYOffset);
			} else {
				PolygonTool::setPolygonOffset(snappedXOffset, snappedYOffset);
			}
			m_gizmoCenter.x = m_gizmoPolygonStartCenter.x + snappedXOffset;
			m_gizmoCenter.y = m_gizmoPolygonStartCenter.y + snappedYOffset;
		}
	} else {
		xOffset = snappedXOffset;
		yOffset = snappedYOffset;
		if (axis == GIZMO_MOVE_Z) {
			m_modifyUndoable->SetZOffset(zOffset);
		} else {
			m_modifyUndoable->SetOffset(xOffset, yOffset);
		}
	}
}

void PointerTool::handleGizmoRotation(CPoint viewPt, WbView* pView, CWorldBuilderDoc* pDoc)
{
	Coord3D cpt;
	pView->viewToDocCoords(viewPt, &cpt);
	
	Real dx = cpt.x - m_gizmoCenter.x;
	Real dy = cpt.y - m_gizmoCenter.y;
	Real currentMouseAngle = atan2(dy, dx);
	
	// Calculate incremental delta from previous mouse position (not from start)
	// This avoids the jump when atan2 wraps from +π to -π
	Real frameDelta = currentMouseAngle - m_gizmoPrevMouseAngle;
	
	if (frameDelta > PI) frameDelta -= 2.0f * PI;
	if (frameDelta < -PI) frameDelta += 2.0f * PI;
	
	m_gizmoAccumulatedDelta += frameDelta;
	m_gizmoPrevMouseAngle = currentMouseAngle;
	
	Real scaledDelta = m_gizmoAccumulatedDelta * 1.5f;
	
	Real newAngle = m_gizmoObjectStartAngle + scaledDelta;
	
	// Snap to configured increment by default, Shift for free manipulation
	Bool shiftKey = (0x8000 & ::GetAsyncKeyState(VK_SHIFT)) != 0;
	Real degrees = newAngle * 180.0f / PI;
	if (!shiftKey && s_rotationSnapDegrees > 0) {
		Real halfSnap = s_rotationSnapDegrees * 0.5f;
		degrees = floor((degrees + halfSnap) / s_rotationSnapDegrees) * s_rotationSnapDegrees;
	}
	newAngle = degrees * PI / 180.0f;
	
	m_gizmoRotationDelta = newAngle - m_gizmoObjectStartAngle;
	
	if (m_gizmoIsPolygon) {
		PolygonTool::rotateSelectedPolygon(frameDelta * 1.5f, m_gizmoCenter);
		updateGizmoCenter();
	} else if (m_modifyUndoable) {
		m_modifyUndoable->RotateTo(newAngle);
	}
	
	Real deltaDegrees = m_gizmoRotationDelta * 180.0f / PI;
	CString str;
	if (s_rotationSnapDegrees > 0) {
		str.Format("Rotation: %.0f degrees (Shift for free rotation, snap: %.0f)", deltaDegrees, s_rotationSnapDegrees);
	} else {
		str.Format("Rotation: %.1f degrees", deltaDegrees);
	}
	CMainFrame::GetMainFrame()->SetMessageText(str);
}

void PointerTool::setGizmoMode(GizmoMode mode)
{
	m_gizmoMode = mode;
	updateGizmoCenter();
	
	CWorldBuilderDoc *pDoc = CWorldBuilderDoc::GetActiveDoc();
	if (pDoc) {
		WbView3d *p3View = pDoc->Get3DView();
		if (p3View) {
			p3View->Invalidate(false);
		}
	}
}

