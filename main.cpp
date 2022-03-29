//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "main.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "TopexThread.h"
TopexThread *threadTopex;

#include "ThreadJason.h"
JasonThread *threadJason1;

#include "ThreadEnvisat.h"
EnviThread *threadEnvi;
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma resource "*.dfm"
TAlt_start *Alt_start;
//---------------------------------------------------------------------------
__fastcall TAlt_start::TAlt_start(TComponent* Owner)
        : TForm(Owner)
{
   Status_help->Items->Add("Creating connection to PostgreSQL...");
   LIBPQ=LoadLibrary("libpq.dll");

   if (LIBPQ)
   {
        if (!PQconnectdb)
                PQconnectdb = (PGconn*(__stdcall *) (const char *))GetProcAddress(LIBPQ, "PQconnectdb");
        if (!PQfinish)
                PQfinish = (void (__stdcall *) (PGconn *))GetProcAddress(LIBPQ, "PQfinish");
        if (!PQstatus)
                PQstatus = (ConnStatusType (__stdcall *) (const PGconn *))GetProcAddress(LIBPQ, "PQstatus");
        if (!PQerrorMessage)
                PQerrorMessage = (char*(__stdcall *) (const PGconn *))GetProcAddress(LIBPQ, "PQerrorMessage");
        if (!PQexec)
                PQexec = (PGresult*(__stdcall *) (PGconn *,const char*))GetProcAddress(LIBPQ, "PQexec");
        if (!PQresultStatus)
                PQresultStatus = (ExecStatusType(__stdcall *) (const PGresult *))GetProcAddress(LIBPQ, "PQresultStatus");
        if (!PQclear)
                PQclear = (void(__stdcall *) (PGresult *))GetProcAddress(LIBPQ, "PQclear");
        AnsiString connString;
        connString.printf("host = %s dbname = %s user = %s",PE_server->Text,PE_dbname->Text,PE_user->Text);
//        conn = PQconnectdb("host = gatina dbname = alt user = postgres");
        conn = PQconnectdb(connString.c_str());
        if (PQstatus(conn) == CONNECTION_OK)
        {
           Status_help->Items->Add("Connected to PostgreSQL.");
           Status_help->Items->Add("Enter paths to TLE and MGDR data");
        }
        else
        {
           Button_mgdr->Enabled=false;
           Button_tle->Enabled=false;
           Button_start->Enabled=false;
           MessageDlg(PQerrorMessage(conn),mtError,TMsgDlgButtons() << mbOK,0);
           Status_help->Items->Add("Connection to PostgreSQL failed. Restart");
        }

   }
}
//---------------------------------------------------------------------------
void __fastcall TAlt_start::Button_exitClick(TObject *Sender)
{
Application->Terminate();
}
//---------------------------------------------------------------------------
//              событие по клику на кнопку MGDR
void __fastcall TAlt_start::Button_mgdrClick(TObject *Sender)
{
        if (OpenDialog_mgdr->Execute())
        {
                Button_mgdr->Caption=GetCurrentDir();
                Status_help->Items->Add(Buffer1.sprintf("Path to MGDR data change to '%s'",Button_mgdr->Caption));
        }
}
//---------------------------------------------------------------------------
//              событие по клику на кнопку Tle
void __fastcall TAlt_start::Button_tleClick(TObject *Sender)
{
        if (OpenDialog_tle->Execute())
        {
                Button_tle->Caption=GetCurrentDir();
                Status_help->Items->Add(Buffer1.sprintf("Path to TLE data change to '%s'",Button_tle->Caption));
        }
}
void __fastcall TAlt_start::Button_startClick(TObject *Sender)
{
   if ((Button_mgdr->Caption=="Choose directory") || (Button_tle->Caption=="Choose directory"))
        MessageDlg("Choose directory",mtWarning,TMsgDlgButtons() << mbOK,0);
   else
   {
        Status_help->Items->Add("Prepearing...");
        Button_mgdr->Enabled=false;
        Button_tle->Enabled=false;
        Button_start->Enabled=false;
        Button_exit->Visible=false;
        Button_cancel->Visible=true;

        if (Radio_calc_topex->Checked)
        {
                Status_help->Items->Add("Calculating MGDR-B data of Topex/Poseidon");
                threadTopex=new TopexThread(false);
                threadTopex->OnTerminate=ThreadDone;
        }
        if (Radio_calc_jason1->Checked)
        {
                Status_help->Items->Add("Calculating GDR data of Jason1");
                threadJason1=new JasonThread(false);
                threadJason1->OnTerminate=ThreadDone;
        }
        if (Radio_calc_envi->Checked)
        {
                threadEnvi=new EnviThread(false);
                threadEnvi->OnTerminate=ThreadDone;
        }

   }
}

//---------------------------------------------------------------------------
void __fastcall TAlt_start::ThreadDone(TObject *Sender)
{
        Button_mgdr->Enabled=true;
        Button_tle->Enabled=true;
        Button_start->Enabled=true;
        Button_exit->Visible=true;
        Button_cancel->Visible=false;
        Status_help->Items->Add("Good luck");
}
//---------------------------------------------------------------------------

void __fastcall TAlt_start::Button_cancelClick(TObject *Sender)
{
        MessageDlg("Прерывание программы может привести\nк нежелательным последствиям!",mtWarning,TMsgDlgButtons() << mbOK,0);
}
//---------------------------------------------------------------------------

void __fastcall TAlt_start::Button_testconnectClick(TObject *Sender)
{
   Status_help->Items->Add("Creating connection to PostgreSQL...");
   LIBPQ=LoadLibrary("libpq.dll");

   if (LIBPQ)
   {
        if (!PQconnectdb)
                PQconnectdb = (PGconn*(__stdcall *) (const char *))GetProcAddress(LIBPQ, "PQconnectdb");
        if (!PQfinish)
                PQfinish = (void (__stdcall *) (PGconn *))GetProcAddress(LIBPQ, "PQfinish");
        if (!PQstatus)
                PQstatus = (ConnStatusType (__stdcall *) (const PGconn *))GetProcAddress(LIBPQ, "PQstatus");
        if (!PQerrorMessage)
                PQerrorMessage = (char*(__stdcall *) (const PGconn *))GetProcAddress(LIBPQ, "PQerrorMessage");
        if (!PQexec)
                PQexec = (PGresult*(__stdcall *) (PGconn *,const char*))GetProcAddress(LIBPQ, "PQexec");
        if (!PQresultStatus)
                PQresultStatus = (ExecStatusType(__stdcall *) (const PGresult *))GetProcAddress(LIBPQ, "PQresultStatus");
        if (!PQclear)
                PQclear = (void(__stdcall *) (PGresult *))GetProcAddress(LIBPQ, "PQclear");
        AnsiString connString;
        connString.printf("host = %s dbname = %s user = %s",PE_server->Text,PE_dbname->Text,PE_user->Text);
//        conn = PQconnectdb("host = gatina dbname = alt user = postgres");
        conn = PQconnectdb(connString.c_str());
        if (PQstatus(conn) == CONNECTION_OK)
        {
           Status_help->Items->Add("Connected to PostgreSQL.");
           Status_help->Items->Add("Enter paths to TLE and MGDR data");
           Button_mgdr->Enabled=true;
           Button_tle->Enabled=true;
           Button_start->Enabled=true;
        }
        else
        {
           Button_mgdr->Enabled=false;
           Button_tle->Enabled=false;
           Button_start->Enabled=false;
           MessageDlg(PQerrorMessage(conn),mtError,TMsgDlgButtons() << mbOK,0);
           Status_help->Items->Add("Connection to PostgreSQL failed. Restart");
        }

   }
}
//---------------------------------------------------------------------------


void __fastcall TAlt_start::Radio_calc_jason1Click(TObject *Sender)
{
Button_tle->Caption="d:/data/tle2005";
Button_mgdr->Caption="d:/data/jason";
GroupBox1->Caption="GDR data";
Label_Mgdr->Caption="Path to Jason1 data";
}
//---------------------------------------------------------------------------

void __fastcall TAlt_start::Radio_calc_topexClick(TObject *Sender)
{
Button_tle->Caption="d:/data/tle";
Button_mgdr->Caption="d:/data/topex";
GroupBox1->Caption="MGDR data";
Label_Mgdr->Caption="Path to Topex data";
}

//---------------------------------------------------------------------------
void __fastcall TAlt_start::Radio_calc_enviClick(TObject *Sender)
{
Button_tle->Caption="d:/data/tle";
Button_mgdr->Caption="d:/data/envi";
GroupBox1->Caption="GDR data";
Label_Mgdr->Caption="Path to Envisat data";
}
//---------------------------------------------------------------------------

