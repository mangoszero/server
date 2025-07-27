//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "dbcedit.h"
#include "stdio.h"
#include "IdGlobal.hpp"
#include <inifiles.hpp>
#include "TitleFrm.h"
#include <dir.h>
#include "thOpenSource.h"
#include "SearchFrm.h"

//---------------------------------------------------------------------------

#pragma package(smart_init)
#pragma resource "*.dfm"
TFrmMain* FrmMain;

//---------------------------------------------------------------------------

__fastcall TFrmMain::TFrmMain(TComponent* Owner)
    : TForm(Owner)
{
    OpenOk = false;
    Term = false;
}

//---------------------------------------------------------------------------

void __fastcall TFrmMain::btOpenClick(TObject* Sender)
{
    if (OpenDialog1->Execute())
    {
        if (FileExists(OpenDialog1->FileName))
        {
            OpenOk = false;
            if (thOpen)
            {
                thOpen->Terminate();
            }
            thOpen = new thOpenFile(false);
            pnFileName->Caption = OpenDialog1->FileName;
        }
        else
        {
            OpenOk = false;
            pnFileName->Caption = "";
        }
    }
}

//---------------------------------------------------------------------------

void __fastcall TFrmMain::btSaveClick(TObject* Sender)
{
    if (OpenOk)
    {
        SaveToFile(CurrentOpenFile.c_str());
    }
    else
    {
        ShowMessage("File not opened completely!");
    }
}

//---------------------------------------------------------------------------

void TFrmMain::SaveToFile(const char* pszFileName)
{
    char szFileName[255];
    FILE* stream;

    fnsplit(pszFileName, 0, 0, szFileName, 0);
    strcat(szFileName, "_new.dbc");

    AnsiString NewFileName = ExtractFilePath(Application->ExeName) + szFileName;
    int iFileHandle;
    AnsiString iniSetFile = ExtractFilePath(Application->ExeName) + "BcdEditer.ini";
    AnsiString SectionName = ExtractFileName(CurrentOpenFile);

    DWORD w;

    CopyFileTo(pszFileName, NewFileName);

    iFileHandle = FileOpen(NewFileName, fmOpenRead | fmOpenWrite);

    if ((stream = fopen(CurrentOpenFile.c_str(), "r+")) == NULL)
    {
        ShowMessage("Error opening file");
        return;
    }

    int iVal;
    float fVal;
    bool isFloat;
    int ColType;

    FileSeek(iFileHandle, 0x14, 0);
    TIniFile* ini = new TIniFile(iniSetFile);

    for (int i = 1; i < sgEdit->RowCount; i++)
    {
        for (int j = 1; j < sgEdit->ColCount; j++)
        {
            if (j == 1) // ID
            {
                iVal = StrToInt(sgEdit->Cells[j][i]);
                FileWrite(iFileHandle, &iVal, 4);
            }
            else
            {
                switch (thOpen->ColType[j])
                {
                    case 0: // Integer
                        iVal = StrToFloat(sgEdit->Cells[j][i]);
                        FileWrite(iFileHandle, &iVal, 4);
                        break;
                    case 1: // Float
                        fVal = StrToFloat(sgEdit->Cells[j][i]);
                        FileWrite(iFileHandle, &fVal, 4);
                        break;
                    case 2: // Text
                        fseek(stream, 0x14 + (i * (sgEdit->ColCount - 1) + (j - 1)) * 4, 0);
                        fread(&iVal, 4, 1, stream);
                        FileWrite(iFileHandle, &iVal, 4);
                        break;
                    default: // Integer
                        iVal = StrToFloat(sgEdit->Cells[j][i]);
                        FileWrite(iFileHandle, &iVal, 4);
                }
            }
        }
    }
    FileClose(iFileHandle);
    fclose(stream);
    delete ini;
    ShowMessage("Saved to file: " + NewFileName);
}
void __fastcall TFrmMain::btIntTypeClick(TObject* Sender)
{
    if (OpenOk == true)
    {
        AnsiString  iniSetFile = ExtractFilePath(Application->ExeName) + "BcdEditer.ini";
        AnsiString SectionName = ExtractFileName(CurrentOpenFile);
        TIniFile* ini;
        ini = new TIniFile(iniSetFile);
        ini->WriteInteger(SectionName, "ColType" + IntToStr(sgEdit->Col), 0);
        delete ini;
        thOpen->ColType[sgEdit->Col] = 0;
        OpenFileCol(CurrentOpenFile, sgEdit->Col, 0);
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::btFloatTypeClick(TObject* Sender)
{
    if (OpenOk == true)
    {
        AnsiString  iniSetFile = ExtractFilePath(Application->ExeName) + "BcdEditer.ini";
        AnsiString SectionName = ExtractFileName(CurrentOpenFile);
        TIniFile* ini;
        ini = new TIniFile(iniSetFile);
        ini->WriteInteger(SectionName, "ColType" + IntToStr(sgEdit->Col), 1);
        delete ini;
        thOpen->ColType[sgEdit->Col] = 1;
        OpenFileCol(CurrentOpenFile, sgEdit->Col, 1);
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::PopupMenu1Popup(TObject* Sender)
{
    if (OpenOk == true)
    {
        AnsiString  iniSetFile = ExtractFilePath(Application->ExeName) + "BcdEditer.ini";
        AnsiString SectionName = ExtractFileName(CurrentOpenFile);
        int ColType;
        TIniFile* ini;
        ini = new TIniFile(iniSetFile);
        ColType = ini->ReadInteger(SectionName, "ColType" + IntToStr(sgEdit->Col), 0);
        delete ini;
        switch (ColType)
        {
            case 0:
                btIntType->Checked = true;
                btFloatType->Checked = false;
                btTxtType->Checked = false;
                break;
            case 1:
                btIntType->Checked = false;
                btFloatType->Checked = true;
                btTxtType->Checked = false;
                break;
            case 2:
                btIntType->Checked = false;
                btFloatType->Checked = false;
                btTxtType->Checked = true;
                break;
            default:
                btIntType->Checked = true;
                btFloatType->Checked = false;
        }
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::N1Click(TObject* Sender)
{
    AnsiString  iniSetFile = ExtractFilePath(Application->ExeName) + "BcdEditer.ini";
    AnsiString SectionName = ExtractFileName(CurrentOpenFile);
    int ColType;
    FrmTitle->edTitle->Text = sgEdit->Cells[sgEdit->Col][0];
    if (FrmTitle->ShowModal() == mrOk)
    {
        TIniFile* ini;
        ini = new TIniFile(iniSetFile);
        ini->WriteString(SectionName, "ColTitle" + IntToStr(sgEdit->Col), FrmTitle->edTitle->Text);
        delete ini;
        sgEdit->Cells[sgEdit->Col][0] = FrmTitle->edTitle->Text;
    }
}
//---------------------------------------------------------------------------



void __fastcall TFrmMain::FormDestroy(TObject* Sender)
{
    if (thOpen)
    {
        thOpen->Terminate();
        SleepEx(200, 0);
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::ToolButton1Click(TObject* Sender)
{
    bool SeFlag = true;
    if (FrmSearch->ShowModal() == mrOk)
    {
        switch (FrmSearch->rgSI->ItemIndex)
        {
            case 0: //Integer value;
                for (int i = sgEdit->ColCount * sgEdit->Row + sgEdit->Col - 1; i > sgEdit->ColCount; i--)
                {
                    if (i % sgEdit->ColCount != 0)
                    {
                        if (0 == CompareStr(sgEdit->Cells[i - sgEdit->ColCount * (i / sgEdit->ColCount)][i / sgEdit->ColCount],
                                            FrmSearch->edSeach->Text))  //Found it
                        {
                            sgEdit->Col = i - sgEdit->ColCount * i / sgEdit->ColCount;
                            sgEdit->Row = i / sgEdit->ColCount;
                            SeFlag = false;
                            break;
                        }
                    }
                }
                if (SeFlag)  ShowMessage("Seach Top£¬Find Nothing.");
                {
                    break;
                }
            case 1:  //Search downward;
                for (int i = sgEdit->ColCount * sgEdit->Row + sgEdit->Col + 1; i < sgEdit->ColCount * sgEdit->RowCount; i++)
                {
                    if (i % sgEdit->ColCount != 0)
                    {
                        if (0 == CompareStr(sgEdit->Cells[i - sgEdit->ColCount * (i / sgEdit->ColCount)][i / sgEdit->ColCount],
                                            FrmSearch->edSeach->Text))  //Found it
                        {
                            sgEdit->Col = i - sgEdit->ColCount * (i / sgEdit->ColCount);
                            sgEdit->Row = i / sgEdit->ColCount;
                            SeFlag = false;
                            break;
                        }
                    }
                }
                if (SeFlag)  ShowMessage("Seach End£¬Find Nothing");
                break;
            case 2: //Search upward in current column;
                for (int i = sgEdit->Row; i > 1; i--)
                {
                    if (0 == CompareStr(sgEdit->Cells[sgEdit->Col][i],
                                        FrmSearch->edSeach->Text))  //Found it
                    {
                        sgEdit->Row = i;
                        SeFlag = false;
                        break;
                    }
                }
                if (SeFlag)  ShowMessage("Seach col top£¬Find Nothing");
                break;
            case 3:  //Search upward in current column;
                for (int i = sgEdit->Row; i < sgEdit->RowCount; i++)
                {
                    if (0 == CompareStr(sgEdit->Cells[sgEdit->Col][i],
                                        FrmSearch->edSeach->Text))  //Found it
                    {
                        sgEdit->Row = i;
                        SeFlag = false;
                        break;
                    }
                }
                if (SeFlag)  ShowMessage("Seach col end£¬Find Nothing.");
                break;
        }
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::sgEditKeyDown(TObject* Sender, WORD& Key,
                                        TShiftState Shift)
{

    bool SeFlag = true;
    if (Key == VK_F3)
    {
        switch (FrmSearch->rgSI->ItemIndex)
        {
            case 0: //Integer value;
                for (int i = sgEdit->ColCount * sgEdit->Row + sgEdit->Col - 1; i > sgEdit->ColCount; i--)
                {
                    if (i % sgEdit->ColCount != 0)
                    {
                        if (0 == CompareStr(sgEdit->Cells[i - sgEdit->ColCount * (i / sgEdit->ColCount)][i / sgEdit->ColCount],
                                            FrmSearch->edSeach->Text))  //Found it
                        {
                            sgEdit->Col = i - sgEdit->ColCount * i / sgEdit->ColCount;
                            sgEdit->Row = i / sgEdit->ColCount;
                            SeFlag = false;
                            break;
                        }
                    }
                }
                if (SeFlag)  ShowMessage("Seach Top£¬Find Nothing.");
                break;
            case 1:  //Search downward;
                for (int i = sgEdit->ColCount * sgEdit->Row + sgEdit->Col + 1; i < sgEdit->ColCount * sgEdit->RowCount; i++)
                {
                    if (i % sgEdit->ColCount != 0)
                    {
                        if (0 == CompareStr(sgEdit->Cells[i - sgEdit->ColCount * (i / sgEdit->ColCount)][i / sgEdit->ColCount],
                                            FrmSearch->edSeach->Text))  //Found it
                        {
                            sgEdit->Col = i - sgEdit->ColCount * (i / sgEdit->ColCount);
                            sgEdit->Row = i / sgEdit->ColCount;
                            SeFlag = false;
                            break;
                        }
                    }
                }
                if (SeFlag)  ShowMessage("Seach End£¬Find Nothing.");
                break;
            case 2: //Search upward in current column;
                for (int i = sgEdit->Row; i > 1; i--)
                {
                    if (0 == CompareStr(sgEdit->Cells[sgEdit->Col][i],
                                        FrmSearch->edSeach->Text))  //Found it
                    {
                        sgEdit->Row = i;
                        SeFlag = false;
                        break;
                    }
                }
                if (SeFlag)  ShowMessage("Seach col Top£¬Find Nothing.");
                break;
            case 3:  //Search upward in current column;
                for (int i = sgEdit->Row; i < sgEdit->RowCount; i++)
                {
                    if (0 == CompareStr(sgEdit->Cells[sgEdit->Col][i],
                                        FrmSearch->edSeach->Text))  //Found it
                    {
                        sgEdit->Row = i;
                        SeFlag = false;
                        break;
                    }
                }
                if (SeFlag)  ShowMessage("Seach col end£¬Find Nothing.");
                break;
        }
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::sgEditSelectCell(TObject* Sender, int ACol,
        int ARow, bool& CanSelect)
{
//
}
//---------------------------------------------------------------------------
void __fastcall TFrmMain::OpenFileCol(AnsiString FileName, int ColIndex, int ColType)
{
    int iFileHandle; //file handle
    char Txtbuf[255];
    int iVal;
    float fVal;
    FILE* stream;
    long curpos, length;
    DWORD dwRows, dwCols, dwRowLen, dwTextLen;

    DWORD dwTextStartPos;
    char* pTextPtr ;


    if ((stream = fopen(FileName.c_str(), "r+"))
            == NULL)
    {
        ShowMessage("Open File Error");
        return;
    }

    curpos = ftell(stream);
    fseek(stream, 0L, SEEK_END);
    length = ftell(stream);


    switch (ColType)
    {
        case 0: //Integer value   Int
            for (int i = 0; i < sgEdit->RowCount - 1; i++)
            {
                fseek(stream, 0x14 + (i * (sgEdit->ColCount - 1) + (ColIndex - 1)) * 4, 0);
                fread(&iVal, 4, 1, stream);
                sgEdit->Cells[ColIndex][i + 1] = IntToStr(iVal);
            }
            break;
        case 1: //Floating point value    Float
            for (int i = 0; i < sgEdit->RowCount - 1; i++)
            {
                fseek(stream, 0x14 + (i * (sgEdit->ColCount - 1) + (ColIndex - 1)) * 4, 0);
                fread(&fVal, 4, 1, stream);
                sgEdit->Cells[ColIndex][i + 1] = FloatToStr(fVal);
            }
            break;
        case 2: //Text      Text
            fseek(stream, 0x4, 0);
            fread(&iVal, 4, 1, stream);
            dwRows = iVal;
            fread(&iVal, 4, 1, stream);
            dwCols = iVal;
            fread(&iVal, 4, 1, stream);
            dwRowLen = iVal;
            fread(&iVal, 4, 1, stream);
            dwTextLen = iVal;

            dwTextStartPos = dwRows * dwRowLen + 20;
            for (int i = 0; i < sgEdit->RowCount - 1; i++)
            {
                fseek(stream, 0x14 + (i * (sgEdit->ColCount - 1) + (ColIndex - 1)) * 4, 0);
                fread(&iVal, 4, 1, stream);
                sgEdit->Cells[ColIndex][i + 1] = IntToStr(iVal);
                if (dwTextStartPos + iVal < length)
                {
                    fseek(stream, dwTextStartPos + iVal, 0);
                    fread(Txtbuf, 1, 255, stream);
                    //pTextPtr = pBuff + dwTextStartPos + lTemp;
                    sgEdit->Cells[ColIndex][i + 1] = Txtbuf;
                }
                else
                {
                    sgEdit->Cells[ColIndex][i + 1] = "This Col Not Text!";
                }
            }
            break;
    }
    fclose(stream);
}
void __fastcall TFrmMain::Timer1Timer(TObject* Sender)
{
    if (OpenOk)
    {
        lbOpState->Caption = "Open File Ok.";
    }
    else
    {
        lbOpState->Caption = "Open Now.....";
    }
}
//---------------------------------------------------------------------------
//Writing current cell into modified file
void __fastcall TFrmMain::N4Click(TObject* Sender)
{
    if (!thOpen) return;

    int iFileHandle; //file handle
    char buf[4];
    int iVal;
    float fVal;
    FILE* stream;
    /*
       if ((stream = fopen(CurrentOpenFile.c_str(), "r+"))
           == NULL)
       {
          ShowMessage("Error opening file");
          return;
       }
    */
    iFileHandle = FileOpen(CurrentOpenFile, fmOpenRead | fmOpenWrite); //open file

    switch (thOpen->ColType[sgEdit->Col])
    {
        case 0: //Integer value
            //for(int i=0;i<sgEdit->RowCount-1;i++){
            /*
                fseek(stream, 0x14+((sgEdit->Row-1)*(sgEdit->ColCount-1)+(sgEdit->Col-1))*4, 0);
                iVal=StrToInt(sgEdit->Cells[sgEdit->Col][sgEdit->Row]);
                memcpy(buf, &iVal, 4);
                for(int i=0;i<4;i++)
                    fwrite(buf+i, 1, 1, stream);
            */
            iVal = StrToInt(sgEdit->Cells[sgEdit->Col][sgEdit->Row]);
            memcpy(buf, &iVal, 4);
            FileSeek(iFileHandle, 0x14 + ((sgEdit->Row - 1) * (sgEdit->ColCount - 1) + (sgEdit->Col - 1)) * 4, 0);
            FileWrite(iFileHandle, buf, 4);
            //}
            break;
        case 1: //Floating point value
            //fseek(stream, 0x14+((sgEdit->Row-1)*(sgEdit->ColCount-1)+(sgEdit->Col-1))*4, 0);
            //fVal=StrToFloat(sgEdit->Cells[sgEdit->Col][sgEdit->Row]);
            //fwrite(&fVal, 4, 1, stream);
            fVal = StrToFloat(sgEdit->Cells[sgEdit->Col][sgEdit->Row]);
            memcpy(buf, &fVal, 4);
            FileSeek(iFileHandle, 0x14 + ((sgEdit->Row - 1) * (sgEdit->ColCount - 1) + (sgEdit->Col - 1)) * 4, 0);
            FileWrite(iFileHandle, buf, 4);
            break;
        case 2: //Text not written
            break;
    }

    // fclose(stream);
    FileClose(iFileHandle);
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::btTxtTypeClick(TObject* Sender)
{
    if (OpenOk == true)
    {
        AnsiString  iniSetFile = ExtractFilePath(Application->ExeName) + "BcdEditer.ini";
        AnsiString SectionName = ExtractFileName(CurrentOpenFile);
        TIniFile* ini;
        ini = new TIniFile(iniSetFile);
        ini->WriteInteger(SectionName, "ColType" + IntToStr(sgEdit->Col), 2);
        delete ini;
        thOpen->ColType[sgEdit->Col] = 2;
        OpenFileCol(CurrentOpenFile, sgEdit->Col, 2);
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::ToolButton3Click(TObject* Sender)
{
    int OldCol;
    int OldRow;
    OldRow = sgEdit->Row;
    OldCol = sgEdit->Col;
    if (sgEdit->FixedCols == 1)
    {
        sgEdit->FixedCols = 2;
        if (OldCol != 1)
            sgEdit->Col = OldCol;
        sgEdit->Row = OldRow;
    }
    else
    {
        sgEdit->FixedCols = 1;
        sgEdit->Row = OldRow;
        if (OldCol != 2)
            sgEdit->Col = OldCol;
    }
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::btRowSaveClick(TObject* Sender)
{
    if (OpenOk == false) return;

    int iFileHandle; //file handle
    char Txtbuf[255];
    int iVal;
    char buf[4];
    float fVal;
    FILE* stream;
    long curpos, length;
    DWORD dwRows, dwCols, dwRowLen, dwTextLen;

    DWORD dwTextStartPos;
    char* pTextPtr ;


    //if ((stream = fopen(CurrentOpenFile.c_str(), "r+"))
    //    == NULL)
    //{
    //   ShowMessage("Error opening file");
    //   return;
    //}

    //curpos = ftell(stream);
    //fseek(stream, 0L, SEEK_END);
    //length = ftell(stream);
    iFileHandle = FileOpen(CurrentOpenFile, fmOpenRead | fmOpenWrite); //open file

    for (int i = 0; i < sgEdit->ColCount - 1; i++)
    {
        switch (thOpen->ColType[i])
        {
            case 0: //Integer value             sgEdit->Row
                //fseek(stream, 0x14+((sgEdit->Row-1)*(sgEdit->ColCount-1)+i)*4, 0);
                //iVal=StrToInt(sgEdit->Cells[i+1][sgEdit->Row]);
                //fwrite(&iVal, 4, 1, stream);
                iVal = StrToInt(sgEdit->Cells[i + 1][sgEdit->Row]);
                memcpy(buf, &iVal, 4);
                FileSeek(iFileHandle, 0x14 + ((sgEdit->Row - 1) * (sgEdit->ColCount - 1) + i) * 4, 0);
                FileWrite(iFileHandle, buf, 4);
                break;
            case 1: //Floating point value
                //fseek(stream, 0x14+((sgEdit->Row-1)*(sgEdit->ColCount-1)+i)*4, 0);
                //fVal=StrToFloat(sgEdit->Cells[i+1][sgEdit->Row]);
                //fwrite(&fVal, 4, 1, stream);
                fVal = StrToFloat(sgEdit->Cells[i + 1][sgEdit->Row]);
                memcpy(buf, &fVal, 4);
                FileSeek(iFileHandle, 0x14 + ((sgEdit->Row - 1) * (sgEdit->ColCount - 1) + i) * 4, 0);
                FileWrite(iFileHandle, buf, 4);
                break;
            case 2: //Text   Text
                break;
        }
    }
    //fclose(stream);
    FileClose(iFileHandle);
    ShowMessage("The " + IntToStr(sgEdit->Row) + " Row Write Ok!");
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::btColSaveClick(TObject* Sender)
{
    if (OpenOk == false) return;

    int iFileHandle; //file handle
    char Txtbuf[255];
    int iVal;
    char buf[4];
    float fVal;
    FILE* stream;
    long curpos, length;
    DWORD dwRows, dwCols, dwRowLen, dwTextLen;

    DWORD dwTextStartPos;
    char* pTextPtr ;

    iFileHandle = FileOpen(CurrentOpenFile, fmOpenRead | fmOpenWrite); //open file

    //if ((stream = fopen(CurrentOpenFile.c_str(), "r+"))
    //    == NULL)
    //{
    //   ShowMessage("Error opening file");
    //   return;
    //}

    //curpos = ftell(stream);
    //fseek(stream, 0L, SEEK_END);
    //length = ftell(stream);


    switch (thOpen->ColType[sgEdit->Col])
    {
        case 0: //Integer value
            for (int i = 0; i < sgEdit->RowCount - 1; i++)
            {
                //fseek(stream, 0x14+(i*(sgEdit->ColCount-1)+(sgEdit->Col-1))*4, 0);
                //iVal=StrToInt(sgEdit->Cells[sgEdit->Col][i+1]);
                //fwrite(&iVal, 4, 1, stream);
                iVal = StrToInt(sgEdit->Cells[sgEdit->Col][i + 1]);
                memcpy(buf, &iVal, 4);
                FileSeek(iFileHandle, 0x14 + (i * (sgEdit->ColCount - 1) + (sgEdit->Col - 1)) * 4, 0);
                FileWrite(iFileHandle, buf, 4);
            }
            break;
        case 1: //Floating point value
            for (int i = 0; i < sgEdit->RowCount - 1; i++)
            {
                //fseek(stream, 0x14+(i*(sgEdit->ColCount-1)+(sgEdit->Col-1))*4, 0);
                //fVal=StrToFloat(sgEdit->Cells[sgEdit->Col][i+1]);
                //fwrite(&fVal, 4, 1, stream);
                fVal = StrToFloat(sgEdit->Cells[sgEdit->Col][i + 1]);
                memcpy(buf, &fVal, 4);
                FileSeek(iFileHandle, 0x14 + (i * (sgEdit->ColCount - 1) + (sgEdit->Col - 1)) * 4, 0);
                FileWrite(iFileHandle, buf, 4);
            }
            break;
        case 2: //Text   Text
            break;
    }
    //fclose(stream);

    FileClose(iFileHandle);
    ShowMessage("The " + IntToStr(sgEdit->Col) + "Col Write Ok!");
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::btRowClearClick(TObject* Sender)
{
    if (OpenOk == false) return;

    int iFileHandle; //file handle
    char Txtbuf[255];
    int iVal;
    float fVal;
    FILE* stream;
    long curpos, length;
    DWORD dwRows, dwCols, dwRowLen, dwTextLen;

    DWORD dwTextStartPos;
    char* pTextPtr ;


    if ((stream = fopen(CurrentOpenFile.c_str(), "r+"))
            == NULL)
    {
        ShowMessage("Open File Error!");
        return;
    }

    curpos = ftell(stream);
    fseek(stream, 0L, SEEK_END);
    length = ftell(stream);

    for (int i = 1; i < sgEdit->ColCount - 1; i++)
    {
        switch (thOpen->ColType[i])
        {
            case 0: //Integer value             sgEdit->Row
                //fseek(stream, 0x14+(sgEdit->Row*(sgEdit->ColCount-1)+i)*4, 0);
                //iVal=StrToInt(sgEdit->Cells[i+1][sgEdit->Row]);
                //fwrite(&iVal, 4, 1, stream);
                sgEdit->Cells[i + 1][sgEdit->Row] = "0";
                break;
            case 1: //Floating point value
                //fseek(stream, 0x14+(sgEdit->Row*(sgEdit->ColCount-1)+i)*4, 0);
                //fVal=StrToFloat(sgEdit->Cells[i+1][sgEdit->Row]);
                //fwrite(&fVal, 4, 1, stream);
                sgEdit->Cells[i + 1][sgEdit->Row] = "0";
                break;
            case 2: //Text   Text
                break;
        }
    }
    fclose(stream);
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::btColClearClick(TObject* Sender)
{
    if (OpenOk == false) return;

    int iFileHandle; //file handle
    char Txtbuf[255];
    int iVal;
    float fVal;
    FILE* stream;
    long curpos, length;
    DWORD dwRows, dwCols, dwRowLen, dwTextLen;

    DWORD dwTextStartPos;
    char* pTextPtr ;


    if ((stream = fopen(CurrentOpenFile.c_str(), "r+"))
            == NULL)
    {
        ShowMessage("Open File Error!");
        return;
    }

    curpos = ftell(stream);
    fseek(stream, 0L, SEEK_END);
    length = ftell(stream);


    switch (thOpen->ColType[sgEdit->Col])
    {
        case 0: //Integer value
            for (int i = 0; i < sgEdit->RowCount - 1; i++)
            {
                //fseek(stream, 0x14+(i*(sgEdit->ColCount-1)+(ColIndex-1))*4, 0);
                //iVal=StrToInt(sgEdit->Cells[ColIndex][i+1]);
                //fwrite(&iVal, 4, 1, stream);
                sgEdit->Cells[sgEdit->Col][i + 1] = "0";
            }
            break;
        case 1: //Floating point value
            for (int i = 0; i < sgEdit->RowCount - 1; i++)
            {
                //fseek(stream, 0x14+(i*(sgEdit->ColCount-1)+(ColIndex-1))*4, 0);
                //fVal=StrToFloat(sgEdit->Cells[ColIndex][i+1]);
                //fwrite(&fVal, 4, 1, stream);
                sgEdit->Cells[sgEdit->Col][i + 1] = "0";
            }
            break;
        case 2: //Text   Text
            break;
    }
    fclose(stream);
}
//---------------------------------------------------------------------------

void __fastcall TFrmMain::ToolButton4Click(TObject* Sender)
{
    AnsiString Cmd;
    Cmd = "calc.exe";
    WinExec(Cmd.c_str(), SW_SHOWNORMAL);
}
//---------------------------------------------------------------------------


