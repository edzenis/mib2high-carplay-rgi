package com.luka.carplay.routeguidance;

import com.luka.carplay.framework.Log;

import de.audi.atip.interapp.combi.bap.CombiBAPServiceNavi;
import de.audi.atip.interapp.combi.bap.data.CombiBAPDestinationInfo;
import de.audi.atip.interapp.combi.bap.data.CombiBAPDestinationListEntry;
import de.audi.atip.interapp.combi.bap.data.CombiBAPNaviLaneGuidanceData;
import de.audi.atip.interapp.combi.bap.data.CombiBAPNaviManeuverDescriptor;
import de.audi.atip.interapp.combi.bap.data.CombiBAPSemiDynamicRouteInfo;
import de.audi.atip.interapp.combi.bap.data.CombiBAPTMCInfoMessage;

public final class K2161GatedCombiService
    implements CombiBAPServiceNavi {

    private final CombiBAPServiceNavi real;
    private volatile boolean blockRouteGuidance;

    public K2161GatedCombiService(CombiBAPServiceNavi real) {
        if (real == null) {
            throw new IllegalArgumentException("real=null");
        }

        this.real = real;
    }

    private void note(String s) {
        Log.d("RGI-GATE", s);
    }

    private void blocked(String method) {
        note("BLOCK method=" + method);
    }

    public CombiBAPServiceNavi getRealService() {
        return real;
    }

    public void setRouteGuidanceBlocked(boolean blocked) {
        blockRouteGuidance = blocked;

        note(
            "ROUTE_BLOCK=" +
            (blocked ? "1" : "0")
        );
    }

    public boolean isRouteGuidanceBlocked() {
        return blockRouteGuidance;
    }

    /*
     * ===== NATIVE ROUTE-GUIDANCE WRITES =====
     */

    public void updateRGStatus(int a) {
        if (blockRouteGuidance) {
            blocked("updateRGStatus");
            return;
        }
        real.updateRGStatus(a);
    }

    public void updateRGStatusAndActiveRGType(int a, int b) {
        if (blockRouteGuidance) {
            blocked("updateRGStatusAndActiveRGType");
            return;
        }
        real.updateRGStatusAndActiveRGType(a, b);
    }

    public void updateActiveRGType(int a) {
        if (blockRouteGuidance) {
            blocked("updateActiveRGType");
            return;
        }
        real.updateActiveRGType(a);
    }

    public void updateActiveRGTypeAndRGStatus(int a, int b) {
        if (blockRouteGuidance) {
            blocked("updateActiveRGTypeAndRGStatus");
            return;
        }
        real.updateActiveRGTypeAndRGStatus(a, b);
    }

    public void updateDistanceToNextManeuver(
        int a,
        int b,
        boolean c,
        int d
    ) {
        if (blockRouteGuidance) {
            blocked("updateDistanceToNextManeuver");
            return;
        }
        real.updateDistanceToNextManeuver(a, b, c, d);
    }

    public void updateManeuverDescriptor(
        CombiBAPNaviManeuverDescriptor[] a
    ) {
        if (blockRouteGuidance) {
            blocked("updateManeuverDescriptor");
            return;
        }
        real.updateManeuverDescriptor(a);
    }

    public void updateManeuverDescriptorAndExitView(
        CombiBAPNaviManeuverDescriptor[] a,
        int b,
        int c
    ) {
        if (blockRouteGuidance) {
            blocked("updateManeuverDescriptorAndExitView");
            return;
        }
        real.updateManeuverDescriptorAndExitView(a, b, c);
    }

    public void updateLaneGuidance(
        boolean a,
        CombiBAPNaviLaneGuidanceData[] b
    ) {
        if (blockRouteGuidance) {
            blocked("updateLaneGuidance");
            return;
        }
        real.updateLaneGuidance(a, b);
    }

    public void updateExitView(int a, int b) {
        if (blockRouteGuidance) {
            blocked("updateExitView");
            return;
        }
        real.updateExitView(a, b);
    }

    public void updateManeuverState(int a) {
        if (blockRouteGuidance) {
            blocked("updateManeuverState");
            return;
        }
        real.updateManeuverState(a);
    }

    /*
     * ===== EVERYTHING ELSE PASSES THROUGH =====
     */

    public void showInitializingScreen() {
        real.showInitializingScreen();
    }

    public void hideInitializingScreen() {
        real.hideInitializingScreen();
    }

    public void updateCompassInfo(int a, int b) {
        real.updateCompassInfo(a, b);
    }

    public void updateCurrentPositionInfo(String a) {
        real.updateCurrentPositionInfo(a);
    }

    public void updateTurnToInfo(String a, String b) {
        real.updateTurnToInfo(a, b);
    }

    public void updateDistanceToDestination(
        int a,
        int b,
        boolean c
    ) {
        real.updateDistanceToDestination(a, b, c);
    }

    public void updateTimeToDestination(
        int a,
        int b,
        long c
    ) {
        real.updateTimeToDestination(a, b, c);
    }

    public void updateTMCInfoMessages(
        CombiBAPTMCInfoMessage[] a
    ) {
        real.updateTMCInfoMessages(a);
    }

    public void responseLastDestinationsList(
        int a,
        CombiBAPDestinationListEntry[] b
    ) {
        real.responseLastDestinationsList(a, b);
    }

    public void updateLastDestinationsList(
        CombiBAPDestinationListEntry[] a
    ) {
        real.updateLastDestinationsList(a);
    }

    public void responseFavoriteDestinationsList(
        int a,
        CombiBAPDestinationListEntry[] b
    ) {
        real.responseFavoriteDestinationsList(a, b);
    }

    public void updateFavoriteDestinationsList(
        CombiBAPDestinationListEntry[] a
    ) {
        real.updateFavoriteDestinationsList(a);
    }

    public void routeGuidanceActDeactResult(int a) {
        real.routeGuidanceActDeactResult(a);
    }

    public void repeatLastNavAnnouncementResult(int a) {
        real.repeatLastNavAnnouncementResult(a);
    }

    public void updateVoiceGuidanceState(int a) {
        real.updateVoiceGuidanceState(a);
    }

    public void updateInfoStates(int a) {
        real.updateInfoStates(a);
    }

    public void updateTrafficBlockIndication(int a) {
        real.updateTrafficBlockIndication(a);
    }

    public void updateMapColor(int a) {
        real.updateMapColor(a);
    }

    public void updateMapType(int a, int b) {
        real.updateMapType(a, b);
    }

    public void updateSupportedMapTypes(
        boolean a,
        int b
    ) {
        real.updateSupportedMapTypes(a, b);
    }

    public void updateMapView(int a, int b) {
        real.updateMapView(a, b);
    }

    public void updateSupportedMapViews(
        int a,
        int b
    ) {
        real.updateSupportedMapViews(a, b);
    }

    public void updateMapVisibility(
        boolean a,
        boolean b
    ) {
        real.updateMapVisibility(a, b);
    }

    public void updateMapOrientation(int a) {
        real.updateMapOrientation(a);
    }

    public void updateMapScale(
        int a,
        boolean b,
        int c,
        int d,
        boolean e
    ) {
        real.updateMapScale(a, b, c, d, e);
    }

    public void updateDestinationInfo(
        CombiBAPDestinationInfo a
    ) {
        real.updateDestinationInfo(a);
    }

    public void updateAltitude(int a, int b) {
        real.updateAltitude(a, b);
    }

    public void updateOnlineNavigationState(
        int a,
        int b,
        int c
    ) {
        real.updateOnlineNavigationState(a, b, c);
    }

    public void updateSemidynamicRouteGuidance(
        CombiBAPSemiDynamicRouteInfo a
    ) {
        real.updateSemidynamicRouteGuidance(a);
    }

    public void poiSearchResult(int a, int b) {
        real.poiSearchResult(a, b);
    }

    public void updatePOIListSize(int a) {
        real.updatePOIListSize(a);
    }

    public void updateFSGSetup(
        int a,
        boolean b
    ) {
        real.updateFSGSetup(a, b);
    }

    public void updateMapPresentation(
        boolean a,
        boolean b,
        boolean c
    ) {
        real.updateMapPresentation(a, b, c);
    }
}
