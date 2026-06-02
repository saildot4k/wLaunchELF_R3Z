//--------------------------------------------------------------
//File name:   editor_rules.c
//--------------------------------------------------------------
#include "editor_private.h"

void editorApplyRules(void)
{
	int i;

	Editor_nChar = TextSize[Active_Window] + 1;

	Top_Height = 0, Top_Width = 0;
	Editor_nRowsNum = 0, Editor_nCharRows = 1, Editor_nRowsTop = 1;
	for (i = 0; i < Editor_nChar; i++)  // Rows Number, Width, Top, Calucations.
	{
		if ((TextMode[Active_Window] == UNIX && TextBuffer[Active_Window][i] == '\r') ||  // Text Mode UNIX End Line.
		    TextBuffer[Active_Window][i] == '\n' ||                                       // Text Mode MAC Or OTHER End Line.
		    Editor_nCharRows >= Rows_Width ||                                             // Line Width > Screen Width.
		    TextBuffer[Active_Window][i] == '\0') {                                       // End Text.
			if (i < Editor_Cur) {
				if ((Editor_nRowsTop += 1) > Rows_Num) {
					Top_Width += Editor_nRowsWidth[Top_Height];
					Top_Height += 1;
				}
			}
			Editor_nRowsWidth[Editor_nRowsNum] = Editor_nCharRows;
			Editor_nRowsNum++;
			Editor_nCharRows = 0;
		}
		if (TextBuffer[Active_Window][i] == '\0')  // End Text Stop Calculations.
			break;
		Editor_nCharRows++;
	}

	if (Editor_Home) {
		Tmp_Cur = 0;
		for (i = 0; i < Editor_nRowsNum; i++)  // Home Rules.
		{
			if ((Tmp_Cur += Editor_nRowsWidth[i]) >= Editor_Cur + 1)
				break;
		}
		Tmp_Cur -= (Editor_nRowsWidth[i] - 1);
		Editor_Cur = Tmp_Cur - 1;
		Editor_Home = 0;
	}

	if (Editor_End) {
		Tmp_Cur = 0;
		for (i = 0; i < Editor_nRowsNum; i++)  // End Rules.
		{
			if ((Tmp_Cur += Editor_nRowsWidth[i]) >= Editor_Cur + 1)
				break;
		}
		Editor_Cur = Tmp_Cur - 1;
		Editor_End = 0;
	}

	if (Editor_PushRows < 0) {
		Tmp_Cur = 0;
		for (i = 0; i < Editor_nRowsNum; i++)  // Line / Page Up Rules.
		{
			if ((Tmp_Cur += Editor_nRowsWidth[i]) >= Editor_Cur + 1)
				break;
		}
		Tmp_Cur -= (Editor_nRowsWidth[i] - 1);
		if (Editor_nRowsWidth[i + 1] <= ((Editor_Cur + 1) - (Tmp_Cur - 1)))
			Editor_Cur = Tmp_Cur + Editor_nRowsWidth[i] + Editor_nRowsWidth[i + 1] - 1 - 1;
		else
			Editor_Cur = Tmp_Cur + Editor_nRowsWidth[i] + ((Editor_Cur + 1) - Tmp_Cur) - 1;

		if (Editor_PushRows++ >= 0)
			Editor_PushRows = 0;

		if (Editor_Cur + 1 >= Editor_nChar && TextBuffer[Active_Window][Editor_Cur] != '\0') {
			Editor_Cur = Editor_nChar - 1;
			Editor_PushRows = 0;
		}
	}

	if (Editor_PushRows > 0) {
		Tmp_Cur = 0;
		for (i = 0; i < Editor_nRowsNum; i++)  // Line / Page Down Rules.
		{
			if ((Tmp_Cur += Editor_nRowsWidth[i]) >= Editor_Cur + 1)
				break;
		}
		Tmp_Cur -= (Editor_nRowsWidth[i] - 1);

		if (Editor_nRowsWidth[i - 1] <= ((Editor_Cur + 1) - (Tmp_Cur - 1)))
			Editor_Cur = Tmp_Cur - 1 - 1;
		else
			Editor_Cur = Tmp_Cur - Editor_nRowsWidth[i - 1] + ((Editor_Cur + 1) - Tmp_Cur) - 1;

		if (Editor_PushRows-- <= 0)
			Editor_PushRows = 0;

		if (Editor_Cur <= 0) {
			Editor_Cur = 0;
			Editor_PushRows = 0;
		}
	}

	if (Editor_Cur >= Editor_nChar)  // Max Char Number Rules.
		Editor_Cur = Editor_nChar - 1;

	if (Editor_Cur < 0)  // Min Char Number Rules.
		Editor_Cur = 0;

	if (Mark[MARK_ON]) {  // Mark Rules.
		Mark[MARK_SIZE] = Editor_Cur - Mark[MARK_IN];
		if (Mark[MARK_SIZE] > 0) {
			Mark[MARK_PRINT] = Mark[MARK_SIZE];
			if (Mark[MARK_IN] < Top_Width)
				Mark[MARK_PRINT] = Editor_Cur - Top_Width;
		} else if (Mark[MARK_SIZE] < 0)
			Mark[MARK_PRINT] = 1;
		else
			Mark[MARK_PRINT] = 0;
	}
}
//--------------------------------------------------------------
//End of file: editor_rules.c
//--------------------------------------------------------------
