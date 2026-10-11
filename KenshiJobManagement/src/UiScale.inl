// SPDX-License-Identifier: GPL-3.0-only
// LEOPARD fork: one scale for the whole manager (layout and fonts).

#ifndef KENSHI_JOB_MANAGEMENT_UI_SCALE_INL
#define KENSHI_JOB_MANAGEMENT_UI_SCALE_INL

    // LEOPARD: layout constants of the original, now scaled together
    // with the fonts (S() - McmSettings.inl). Called before the window
    // is built.
    void ApplyUiScale()
    {
        ComputeUiScale();
        PAD = S(12);
        TOP_HEIGHT = S(46);
        HEADER_HEIGHT = S(38);
        ACTION_HEIGHT = S(82);
        SCROLL_SIZE = S(33);
        SQUAD_SELECTOR_HEIGHT = S(71);
        SQUAD_SELECTOR_LABEL_WIDTH = S(76);
        SQUAD_SELECTOR_BUTTON_WIDTH = S(150);
        SQUAD_SELECTOR_BUTTON_HEIGHT = S(34);
        SQUAD_SELECTOR_BUTTON_GAP = S(4);
        SQUAD_SELECTOR_SCROLL_HEIGHT = S(33);
        ROW_HEIGHT = S(128);    // LEOPARD: 122 + room for the taller row buttons
        ROW_GAP = S(6);
        CARD_HEIGHT = S(88);
        MEMBER_PORTRAIT_SIZE = S(80);
        MEMBER_PORTRAIT_INSET = S(2);
        MEMBER_TEXT_LEFT = S(94);
        CARD_WIDTH = S(168);
        CARD_GAP = S(4);
        DRAG_EDGE = S(32);
        STATION_GRID_BANNER_HEIGHT = S(38);
        STATION_GRID_SCROLL_SIZE = S(33);
        STATION_GRID_GROUP_HEADER_HEIGHT = S(34);
        STATION_GRID_CARD_WIDTH = S(214);
        STATION_GRID_CARD_HEIGHT = S(128);
        STATION_GRID_GAP = S(10);
        STATION_GRID_SIDE_PAD = S(8);
        STATION_DETAIL_ROW_HEIGHT = S(48);
        STATION_MEMBER_ROW_HEIGHT = S(116);
        SQUAD_GROUP_HEADER_HEIGHT = S(30);
        ROW_STRIDE = ROW_HEIGHT + ROW_GAP;
        CARD_TOP = (ROW_HEIGHT - CARD_HEIGHT) / 2;
        CARD_STRIDE = CARD_WIDTH + CARD_GAP;
        STATION_GRID_OVERSCAN = STATION_GRID_CARD_HEIGHT;
        STATION_HEADER_HEIGHT = STATION_GRID_CARD_HEIGHT;
        SQUAD_GROUP_GAP = ROW_HEIGHT / 2;
    }

#endif
