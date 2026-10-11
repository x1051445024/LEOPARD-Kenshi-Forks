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
        // LEOPARD: the game's large scroll bars (Kenshi_SliderVBig /
        // HBig) - their arrows are drawn at native size, 32 x 27 / 24 x 34.
        SCROLL_SIZE = 34;
        SQUAD_SELECTOR_HEIGHT = S(71);
        SQUAD_SELECTOR_LABEL_WIDTH = S(76);
        SQUAD_SELECTOR_BUTTON_WIDTH = S(150);
        SQUAD_SELECTOR_BUTTON_HEIGHT = S(34);
        SQUAD_SELECTOR_BUTTON_GAP = S(4);
        SQUAD_SELECTOR_SCROLL_HEIGHT = 34;
        FRAME_X = S(3) + 10 + S(8);
        FRAME_Y = S(3) + 10 + S(5);
        FRAME_FILL = S(3) + 6;
        // member row: portrait + four text lines, the two buttons below
        ROW_HEIGHT = S(121) + 2 * FRAME_Y;
        ROW_GAP = S(6);
        // job card: caption (two lines), arrow, target
        CARD_HEIGHT = S(76) + 2 * FRAME_Y;
        MEMBER_PORTRAIT_SIZE = S(80);
        MEMBER_PORTRAIT_INSET = S(2);
        MEMBER_TEXT_LEFT = FRAME_X + MEMBER_PORTRAIT_SIZE + S(10);
        CARD_WIDTH = S(176);
        CARD_GAP = S(4);
        DRAG_EDGE = S(32);
        STATION_GRID_BANNER_HEIGHT = S(38);
        STATION_GRID_SCROLL_SIZE = 34;
        STATION_GRID_GROUP_HEADER_HEIGHT = S(34);
        STATION_GRID_CARD_WIDTH = S(220);
        // name (two lines), area, assignment, status
        STATION_GRID_CARD_HEIGHT = S(116) + 2 * FRAME_Y;
        STATION_GRID_GAP = S(10);
        STATION_GRID_SIDE_PAD = S(8);
        STATION_DETAIL_ROW_HEIGHT = S(48) + 20;
        STATION_MEMBER_ROW_HEIGHT = S(116);
        SQUAD_GROUP_HEADER_HEIGHT = S(24) + 16;
        ROW_STRIDE = ROW_HEIGHT + ROW_GAP;
        CARD_TOP = (ROW_HEIGHT - CARD_HEIGHT) / 2;
        CARD_STRIDE = CARD_WIDTH + CARD_GAP;
        STATION_GRID_OVERSCAN = STATION_GRID_CARD_HEIGHT;
        STATION_HEADER_HEIGHT = STATION_GRID_CARD_HEIGHT;
        SQUAD_GROUP_GAP = ROW_HEIGHT / 2;
    }

#endif
