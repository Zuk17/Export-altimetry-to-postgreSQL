//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop
#include "TopexThread.h"

#include "stdafx.h"
#include "cTle.h"
#include "cEci.h"
#include "cOrbit.h"
#include "cSite.h"


#pragma package(smart_init)
//---------------------------------------------------------------------------

//   Important: Methods and properties of objects in VCL can only be
//   used in a method called using Synchronize, for example:
//
//      Synchronize(UpdateCaption);
//
//   where UpdateCaption could look like:
//
//      void __fastcall TopexThread::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------

__fastcall TopexThread::TopexThread(bool CreateSuspended)
        : TThread(CreateSuspended)
{
FreeOnTerminate=true;
}
//---------------------------------------------------------------------------
void __fastcall TopexThread::Execute()
{
        //---- Place thread code here ----
        DirList=new TStringList;
        ListOut=new TStringList;
        ListOut->Clear();
        dt1958=TDateTime(1958,01,01);
        if (CreatingTleDB(Alt_start->Button_tle->Caption))
        {
                ListOut->Clear();
                Alt_start->Status_help->Items->Add("Start scanning path to MGDR data...");
//                Alt_start->Refresh();
                if (FindFirst(Alt_start->Button_mgdr->Caption+"\\*.*",faAnyFile, sr) == 0)
                {
                        do
                        {
                                if(((sr.Attr & faDirectory)!= 0) && (sr.Name!=".") && (sr.Name!=".."))
                                        ListOut->Add(Alt_start->Button_mgdr->Caption+"\\"+sr.Name);
                        }
                        while (FindNext(sr) == 0);
                        FindClose(sr);
                }
                if (ListOut->Count==0)
                        ListOut->Add(Alt_start->Button_mgdr->Caption);
                ListOut->Sort();
                Alt_start->Progress_mgdr->Min=0;
                Alt_start->Progress_mgdr->Max=ListOut->Count;
                Alt_start->Status_help->Items->Add(Buffer_tmp.sprintf("%d cycles was found.",ListOut->Count));
                Alt_start->Status_help->Items->Add("Prepearing finished. Starting calculation...");
//                Alt_start->Refresh();
                for (int i=0;i<ListOut->Count;i++,Alt_start->Progress_mgdr->Position++)
                {
                        Alt_start->Status_help->Items->Add(Buffer_tmp.sprintf("calculating %s cycle...",ListOut->Strings[i]));
                        try
                        {
                                CalculatingMGDR(ListOut->Strings[i]);
                        }
                        catch(...)
                        {
                                Alt_start->Status_help->Items->Add(Buffer_tmp.sprintf("Cann`t to calculate MGDR at cycle %s",ListOut->Strings[i]));
                        }
                }
                Alt_start->Status_help->Items->Add("Calculation finished. Data saved at PostgreSQL DB.");

//__________finishing calculating___________________
                Alt_start->Progress_mgdr->Position=0;
                Alt_start->Progress_mgdr_dir->Position=0;
                Alt_start->Progress_mgdr_file->Position=0;
                Alt_start->Progress_tle->Position=0;
        }
        else
          MessageDlg("Tle_data_db does not created!\nYou are fool!!!",mtError,TMsgDlgButtons() << mbOK,0);

//__________Освобождение памяти_____________________
        TleData_List.Length=0;
        delete DirList;
        delete ListOut;
        return;

}
//------------------------------------------------------------------------------
/*
#                       MGDR
#       Считывает файлы в каталоге Path_to_DATA
#       Каждый файл считывается с помощью GETPASSHDR и decode
#       Затем для каждой записи запускается AltCount()
#
#
#
*/
void TopexThread::CalculatingMGDR (AnsiString Path_to_DATA)
{
   //Поиск файлов в дериктории
                DirList->Clear();
                if (FindFirst(Path_to_DATA+"\\*.*",faReadOnly | faHidden, sr) == 0)
                {
                        do
                        {
                                DirList->Add(Path_to_DATA+"\\"+sr.Name);
                        }
                        while (FindNext(sr) == 0);
                        FindClose(sr);
                }

                DirList->Sort();
        //Describing Mgdr_progress BAR
                Alt_start->Progress_mgdr_dir->Min=0;
                Alt_start->Progress_mgdr_dir->Max=DirList->Count;
                Alt_start->Progress_mgdr_dir->Position=0;
                int in;
                unsigned char *pszBuffer;

        //Считывание и обработка файла
                for (int i=0;i<DirList->Count;i++,Alt_start->Progress_mgdr_dir->Position++)
                {
                  filelen=0;
                  NumRec=0;


                  bPassHdr=GETPASSHDR(DirList->Strings[i]);

                  if (bPassHdr)
                  {
                        in=FileOpen(DirList->Strings[i],fmOpenRead);

                        filelen=FileSeek(in,0,2);
                        filelen-=Header1;

                        FileSeek(in,0,0);

                        off=0;
                        pszBuffer=new unsigned char [(int)filelen];

                        FileSeek(in,Header1,SEEK_SET);
                        FileRead((int)in,pszBuffer,(int)filelen);
                        FileClose(in);

                        Alt_start->Progress_mgdr_file->Min=0;
                        Alt_start->Progress_mgdr_file->Max=NumRec-1;

                        for (int j=0;j<NumRec;j++,Alt_start->Progress_mgdr_file->Position++)
                        {
                                decode(pszBuffer);
                                AltCount();
                        }
                        Alt_start->Progress_mgdr_file->Position=0;
                        Alt_start->Refresh();
                        delete pszBuffer;
                  }
                }
}

//------------------------------------------------------------------------------
/*
   #Этот модуль считывает все файлы в директории Path_to_Tle
   #просматривает все файлы на вхождение строки характеризующей TOPEX\POSEIDON
   #в TLE данных (по номеру в первой строке)
   #
   # Затем сохраняет всё в массив структур и проверяет его на наличие хотя
   # бы одного корректного вхождения данных
*/
BOOL TopexThread::CreatingTleDB (AnsiString Path_to_Tle)
{
        Alt_start->Status_help->Items->Add("Creating Tle DB...");
 //Alt_start->Refresh();
        DirList->Clear();
        int j=0;
   //Поиск файлов в дериктории
        if (FindFirst(Path_to_Tle+"\\*.*",faReadOnly | faHidden, sr) == 0)
        {
            do
            {
               DirList->Add(Path_to_Tle+"\\"+sr.Name);
            }
            while (FindNext(sr) == 0);
            FindClose(sr);
        }
        DirList->Sort();
  //Создание массива данных TLE для Топекса
        TleData_List.Length=DirList->Count;
        char msg[256],time_tmp[9];
        int yearT2;
  //Describing Tle_progress BAR
        Alt_start->Progress_tle->Min=0;
        Alt_start->Progress_tle->Max=DirList->Count-1;
  //Считывание и обработка файла Norad TLE
        FILE *iin;
        for (int i=0;i<DirList->Count;i++,Alt_start->Progress_tle->Position++)
        {
             if ((iin = fopen(DirList->Strings[i].c_str(), "rt")) == NULL)
                 MessageDlg("Wrong filename",mtError,TMsgDlgButtons() << mbOK,0);
             else
             {
  //Поиск строк для Топекса
                 while (fgets(msg, 256, iin)!=NULL)
                       if (strncmp(msg,"1 22076U",8)==0) break;
  //Занесение строк в массив
                 if (strncmp("1 22076U",msg,8)==0)
                 {
              //Выбор года
                       yearT2=StrToInt(msg[18])*10+StrToInt(msg[19]);
                       if (yearT2<50)
                             TleData_List[i].year=2000+yearT2;
                       else TleData_List[i].year=1900+yearT2;

                       TleData_List[i].day=StrToInt(msg[20])*100+StrToInt(msg[21])*10+StrToInt(msg[22]);
              //Выбор времени в секундах с начала дня
                       for(j=0;j<8;j++) time_tmp[j]=msg[24+j]; time_tmp[8]=0;
                       TleData_List[i].time=StrToInt(time_tmp)*0.000864;
              //Копирование самих строк в массив
                       strcpy(TleData_List[i].str1,msg);
                       fgets(TleData_List[i].str2,256,iin);
                  }
                  else
                     TleData_List[i].year=0; //Если файл не содержит данных по Topex/Poseidon

                  fclose(iin);
              }
        }
        Alt_start->Status_help->Items->Add(Buffer_tmp.sprintf("Tle DB was created. %d Tle files was found",DirList->Count));
        Alt_start->Refresh();
        for (int i=0;i<DirList->Count;++i)
              if (TleData_List[i].year!=0) return true;
        Alt_start->Status_help->Items->Add("It`s not a TLE data files!!! Choose another directory");
        return false;
}

//---------------------------------------------------------------------------
/*
#
#
#
#
#
#
*/
BOOL TopexThread::AltCount(void)
{
AnsiString Buffer;
AnsiString BC;
int k=0;
int h_orb[10],h_range[10],alt_land[10];
int i=0,j=0;
ofstream fout;


/*-----------------------------------------------*/
        latitude=(double)gdrm.Lat_Tra/1000000;
   if (gdrm.Lon_Tra/1000000>=180.0)
        longitude=(double)((double)gdrm.Lon_Tra/1000000-360);
   else
        longitude=(double)gdrm.Lon_Tra/1000000;

     if (((latitude>45) && (latitude<54) && (longitude>117.6) && (longitude<140)) ||
     	 	((latitude>65) && (latitude<68) && (longitude>65) && (longitude<68)))
     {


// Cnes Orbit
	if (gdrm.HP_Sat<=Alt_Max&&gdrm.HP_Sat>=Alt_Min&&gdrm.HP_Sat!=Alt_Def)
        {
		H_SAT=gdrm.HP_Sat;

                for (k=0;k<10;k++)
                        h_orb[k]=H_SAT+gdrm.HP_Sat_Hi_Rate[k];
        }
        else H_SAT=Alt_Def;

//Altimeter range
       	if (gdrm.H_Alt<=Alt_Max&&gdrm.H_Alt>=Alt_Min||gdrm.H_Alt!=Alt_Def)
        {
        	H_Alt=gdrm.H_Alt;             //Altimeter Range

                for (k=0;k<10;k++)
                        h_range[k]=H_Alt+gdrm.H_Alt_SME[k];
        }
        else H_Alt=Alt_Def;


//Ionosphere correction
        if (gdrm.Iono_Corr<=40&&gdrm.Iono_Corr>=-1000)
                Iono_Cor=gdrm.Iono_Corr;
        else Iono_Cor=32767;

        if (gdrm.Iono_Dor<=0&&gdrm.Iono_Dor>=-1000)
                Iono_Dor=gdrm.Iono_Dor;
        else Iono_Dor=32767;

// Dry correction
	if (gdrm.Dry_Corr<=-2000&&gdrm.Dry_Corr>=-3000&&gdrm.Dry_Corr!=32767) //Dry Correction
		Dry_Cor=gdrm.Dry_Corr;
        else Dry_Cor=32767;
// Wet correction
        if (gdrm.Wet_Corr<=-1000&&gdrm.Wet_Corr>=0&&gdrm.Wet_Corr!=32767)
                Wet_Cor=gdrm.Wet_Corr;
        else Wet_Cor=32767;

        if (gdrm.Wet1_Corr<=-1000&&gdrm.Wet1_Corr>=0&&gdrm.Wet1_Corr!=32767)
                Wet1_Cor=gdrm.Wet1_Corr;
        else Wet1_Cor=32767;

        if (gdrm.Wet2_Corr<=-1000&&gdrm.Wet2_Corr>=0&&gdrm.Wet2_Corr!=32767)
                Wet2_Cor=gdrm.Wet2_Corr;
        else Wet2_Cor=32767;

        if (gdrm.Wet_H_Rad<=-1000&&gdrm.Wet_H_Rad>=0&&gdrm.Wet_H_Rad!=32767)
                Wet_H=gdrm.Wet_H_Rad;
        else Wet_H=32767;

// Solid Earth Tide
        if (gdrm.H_Set>=-1000&&gdrm.H_Set<=1000&&gdrm.H_Set!=32767)
                H_SET=gdrm.H_Set;
        else H_SET=32767;

// Geocentric Pole Tide
        if (gdrm.H_Pol<=-100&&gdrm.H_Pol>=100&&gdrm.H_Pol!=127)
                H_POL=gdrm.H_Pol;
        else H_POL=127;

//Geoid
        if (gdrm.H_Geo>-300000&&gdrm.H_Geo<300000&&gdrm.H_Geo!=2147483647)
                H_GEO=gdrm.H_Geo;
        else H_GEO=2147483647;
// Inverse Barometer Effect
        if (gdrm.INV_BAR>-1300&&gdrm.INV_BAR<3105&&gdrm.INV_BAR!=32767)
                INV_BAR=gdrm.INV_BAR;
        else INV_BAR=32767;

        Buffer.sprintf("insert into alt_data_1hz values ('TP',%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,cast('%s' as timestamp),GeomFromText('POINT(%.3f %.3f)',32652),%s,%s)",longitude,latitude,(H_SAT-H_Alt)/10.,Iono_Cor/10.,Iono_Dor/10.,Dry_Cor/10.,Wet_Cor/10.,Wet1_Cor/10.,Wet2_Cor/10.,Wet_H/10.,H_GEO/10.,H_SET/10.,H_POL/10.,INV_BAR/10.,DATETIMEASTIMATION(gdrm.Tim_Moy_1,gdrm.Tim_Moy_2,gdrm.Tim_Moy_3),longitude,latitude,stPassHdr.Cycle_Number,stPassHdr.Pass_Number); //перевод в метры
        Alt_start->res = Alt_start->PQexec(Alt_start->conn, Buffer.c_str());
        if (Alt_start->PQresultStatus(Alt_start->res) != PGRES_COMMAND_OK)
             MessageDlg(Alt_start->PQerrorMessage(Alt_start->conn),mtError,TMsgDlgButtons() << mbOK,0);
        Alt_start->PQclear(Alt_start->res);



//Calculating Time
int year=1958;
int day=1;
int delta=gdrm.Tim_Moy_1;
double time_tmp;

do
{
	if ((year%4)==0)
		day=366;
	else
		day=365;

	delta-=day;

	if (delta>=0)
		year++;
}while (delta>=0);

delta+=day;delta++; //our date is Year + delta (day) + time_tmp (time in minutes)

//load fresh tle data
   stTleData Tle_work={0,0,0,0,0};

   string str0="qwe";
   for (j=0;j<TleData_List.Length;j++)
      if ((TleData_List[j].year==year) && (TleData_List[j].year>=Tle_work.year))
        if ((TleData_List[j].day<delta) || ((TleData_List[j].day==delta)&&(TleData_List[j].time<((gdrm.Tim_Moy_2/1000+gdrm.Tim_Moy_3/1000000)))))
            if ((TleData_List[j].day>Tle_work.day)||((TleData_List[j].day==Tle_work.day)&&(TleData_List[j].time>Tle_work.time)))
                        Tle_work=TleData_List[j];
   /*
   Бля сука нах...заебала эта хуйня со временем!
        Разбираться с cJulian.cpp (73 строка) - ограничение на 366 дней,
        но чтобы на 1 число посчитать - может быть на 10 больше.
        Переделать выбор свежих Tle данных
   */
   if (Tle_work.year==0)
   {
      Buffer.sprintf("Не считается на %d-%d т.к. некорректный выбор fresh tle data",year,delta);
      Alt_start->Status_help->Items->Add(Buffer);
      return true;
   }

           string str1=Tle_work.str1;
           string str2=Tle_work.str2;
           cTle tle1(str0,str1,str2);
           cOrbit       orbit(tle1);
           cEci         eci;
           cCoordGeo    FreshCoord;


           for (i=0;i<10;i++)
           {
                time_tmp=(gdrm.Tim_Moy_2/1000+(gdrm.Tim_Moy_3-gdrm.Dtim_Mil+gdrm.Dtim_Pac*(2*i+0.5))/1000000)/60;

                //время в минутах с эпохи!!! функция возвращает координату в eci
                orbit.getPosition(time_tmp+(delta-Tle_work.day)*1440-Tle_work.time/60, &eci);
                //преобразует координату
                FreshCoord=eci.toGeo();

//                alt_land[i]=h_orb[i]-(h_range[i]+Dry_Cor+Iono_Cor)-H_GEO-H_SET;
                Buffer.sprintf("insert into alt_data_10hz values ('TP',%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,cast('%s' as timestamp),GeomFromText('POINT(%.3f %.3f)',32652),%s,%s)",FreshCoord.m_Lon*180/PI,FreshCoord.m_Lat*180/PI,(h_orb[i]-h_range[i])/10.,Iono_Cor/10.,Iono_Dor/10.,Dry_Cor/10.,Wet_Cor/10.,Wet1_Cor/10.,Wet2_Cor/10.,Wet_H/10.,H_GEO/10.,H_SET/10.,H_POL/10.,INV_BAR/10.,DATETIMEASTIMATION(gdrm.Tim_Moy_1,int(time_tmp*60000),0),FreshCoord.m_Lon*180/PI,FreshCoord.m_Lat*180/PI,stPassHdr.Cycle_Number,stPassHdr.Pass_Number); //перевод в метры
                Alt_start->res = Alt_start->PQexec(Alt_start->conn, Buffer.c_str());
                if (Alt_start->PQresultStatus(Alt_start->res) != PGRES_COMMAND_OK)
                     MessageDlg(Alt_start->PQerrorMessage(Alt_start->conn),mtError,TMsgDlgButtons() << mbOK,0);
                Alt_start->PQclear(Alt_start->res);
           }
return true;
}
else return false;
}
//-----------------------------------------------------------------------------
BOOL TopexThread::GETPASSHDR (AnsiString PassFile)
{
TStringList *stlPassHdr=new TStringList;
int in=0,filelen;

                stlPassHdr->LoadFromFile(PassFile);
                stPassHdr.Prod_Agency_Name=stlPassHdr->Strings[2].SubString(23,stlPassHdr->Strings[2].Pos(";")-23);
                stPassHdr.Prod_Inst_Name=stlPassHdr->Strings[3].SubString(28,stlPassHdr->Strings[3].Pos(";")-28);
                stPassHdr.Source_Name=stlPassHdr->Strings[4].SubString(14,stlPassHdr->Strings[4].Pos(";")-14);
                stPassHdr.Sensor_Name=stlPassHdr->Strings[5].SubString(14,stlPassHdr->Strings[5].Pos(";")-14);
                stPassHdr.Data_Hand_Ref=stlPassHdr->Strings[6].SubString(26,stlPassHdr->Strings[6].Pos(";")-26);
                stPassHdr.Prod_Cr_Start=stlPassHdr->Strings[7].SubString(28,stlPassHdr->Strings[7].Pos(";")-28);
                stPassHdr.Prod_Cr_End=stlPassHdr->Strings[8].SubString(26,stlPassHdr->Strings[8].Pos(";")-26);
                stPassHdr.Gen_Soft_Name=stlPassHdr->Strings[9].SubString(28,stlPassHdr->Strings[9].Pos(";")-28);
                stPassHdr.Build_ID=stlPassHdr->Strings[10].SubString(11,stlPassHdr->Strings[10].Pos(";")-11);
                stPassHdr.Pass_File_Data_Type=stlPassHdr->Strings[11].SubString(22,stlPassHdr->Strings[11].Pos(";")-22);

                stPassHdr.POS_Range_Bias=stlPassHdr->Strings[12].SubString(22,stlPassHdr->Strings[12].Pos(";")-22);
                stPassHdr.Topex_Range_Bias=stlPassHdr->Strings[13].SubString(21,stlPassHdr->Strings[13].Pos(";")-21);
                stPassHdr.T_P_Sigma0_Offset=stlPassHdr->Strings[14].SubString(20,stlPassHdr->Strings[14].Pos(";")-20);
                stPassHdr.NASA_Orbit_FileName=stlPassHdr->Strings[15].SubString(22,stlPassHdr->Strings[15].Pos(";")-22);
                stPassHdr.Orbit_Qual_NASA=stlPassHdr->Strings[16].SubString(18,stlPassHdr->Strings[16].Pos(";")-18);
                stPassHdr.CNES_Orbit_FileName=stlPassHdr->Strings[17].SubString(22,stlPassHdr->Strings[17].Pos(";")-22);
                stPassHdr.Orbit_Qual_CNES=stlPassHdr->Strings[18].SubString(18,stlPassHdr->Strings[18].Pos(";")-18);
                stPassHdr.Topex_Pass_File_ID=stlPassHdr->Strings[19].SubString(21,stlPassHdr->Strings[19].Pos(";")-21);
                stPassHdr.POS_Pass_File_ID=stlPassHdr->Strings[20].SubString(24,stlPassHdr->Strings[20].Pos(";")-24);
                stPassHdr.CORIOTROP_File_ID=stlPassHdr->Strings[21].SubString(20,stlPassHdr->Strings[21].Pos(";")-20);
                stPassHdr.Cycle_Number=stlPassHdr->Strings[22].SubString(15,stlPassHdr->Strings[22].Pos(";")-15);
                stPassHdr.Pass_Number=stlPassHdr->Strings[23].SubString(14,stlPassHdr->Strings[23].Pos(";")-14);
                stPassHdr.Pass_Data_Count=stlPassHdr->Strings[24].SubString(18,stlPassHdr->Strings[24].Pos(";")-18);
                stPassHdr.Rev_Number=stlPassHdr->Strings[25].SubString(13,stlPassHdr->Strings[25].Pos(";")-13);
                stPassHdr.Equator_Long=stlPassHdr->Strings[26].SubString(20,stlPassHdr->Strings[26].Pos(";")-20);
                stPassHdr.Equator_Time=stlPassHdr->Strings[27].SubString(15,stlPassHdr->Strings[27].Pos(";")-15);
                stPassHdr.Time_First_Pt=stlPassHdr->Strings[28].SubString(16,stlPassHdr->Strings[28].Pos(";")-16);
                stPassHdr.Time_Last_Pt=stlPassHdr->Strings[29].SubString(15,stlPassHdr->Strings[29].Pos(";")-15);
                stPassHdr.Time_Epoch=stlPassHdr->Strings[30].SubString(13,stlPassHdr->Strings[30].Pos(";")-13);

                if (StrToInt(stPassHdr.Pass_Number)%2!=0)
                        TrackType='0';
                else
                        TrackType='1';

//                        NumOrbit=StrToInt(stPassHdr.Pass_Number.Trim());
//                        NumCycle=StrToInt(stPassHdr.Cycle_Number.Trim());
//                        NumRec=StrToInt(stPassHdr.Pass_Data_Count.Trim());
//                        DVUCurLon=StrToFloat(stPassHdr.Equator_Long.Trim());
                        StrToIntDef(stPassHdr.Pass_Number.Trim(),NumOrbit);
                        StrToIntDef(stPassHdr.Cycle_Number.Trim(),NumCycle);
                        StrToIntDef(stPassHdr.Pass_Data_Count.Trim(),NumRec);
                        StrToIntDef(stPassHdr.Equator_Long.Trim(),DVUCurLon);

                        GETDATETIMEDVUCUR(stPassHdr.Equator_Time.Trim());
                NumOrbit=StrToInt(ExtractFileExt(PassFile).SubString(2,3));
                AnsiString Temp;
                NumCycle=StrToInt(ExtractFileName(PassFile).SubString(4,3));

                if (NumOrbit%2!=0)
                        TrackType='0';
                else
                        TrackType='1';

                in=FileOpen(PassFile,fmOpenRead);

                filelen=FileSeek(in,0,2);
                filelen-=Header1;

                NumRec=filelen/GDRM_RECSIZE;

                FileClose(in);
        delete stlPassHdr;

return true;
}
//----------------------------------------------------------------------------
/*
#
# декодирует файл MGDR
#
#
#
*/
void TopexThread::decode(unsigned char *buf)
{
//   int i;

//   off = 0;
   memmove(&gdrm.Tim_Moy_1, &buf[off], sizeof(gdrm.Tim_Moy_1));
   off += sizeof(gdrm.Tim_Moy_1);

   memmove(&gdrm.Tim_Moy_2, &buf[off], sizeof(gdrm.Tim_Moy_2));
   off += sizeof(gdrm.Tim_Moy_2);

   memmove(&gdrm.Tim_Moy_3, &buf[off], sizeof(gdrm.Tim_Moy_3));
   off += sizeof(gdrm.Tim_Moy_3);

   memmove(&gdrm.Dtim_Mil, &buf[off], sizeof(gdrm.Dtim_Mil));
   off += sizeof(gdrm.Dtim_Mil);

   memmove(&gdrm.Dtim_Bias, &buf[off], sizeof(gdrm.Dtim_Bias));
   off += sizeof(gdrm.Dtim_Bias);

   memmove(&gdrm.Dtim_Pac, &buf[off], sizeof(gdrm.Dtim_Pac));
   off += sizeof(gdrm.Dtim_Pac);

   memmove(&gdrm.Lat_Tra, &buf[off], sizeof(gdrm.Lat_Tra));
   off += sizeof(gdrm.Lat_Tra);

   memmove(&gdrm.Lon_Tra, &buf[off], sizeof(gdrm.Lon_Tra));
   off += sizeof(gdrm.Lon_Tra);

   memmove(&gdrm.Sat_Alt, &buf[off], sizeof(gdrm.Sat_Alt));
   off += sizeof(gdrm.Sat_Alt);

   memmove(&gdrm.HP_Sat, &buf[off], sizeof(gdrm.HP_Sat));
   off += sizeof(gdrm.HP_Sat);

   memmove(&gdrm.Sat_Alt_Hi_Rate, &buf[off], sizeof(gdrm.Sat_Alt_Hi_Rate));
   off += sizeof(gdrm.Sat_Alt_Hi_Rate);

   memmove(&gdrm.HP_Sat_Hi_Rate, &buf[off], sizeof(gdrm.HP_Sat_Hi_Rate));
   off += sizeof(gdrm.HP_Sat_Hi_Rate);

   memmove(&gdrm.Att_Wvf, &buf[off], sizeof(gdrm.Att_Wvf));
   off += sizeof(gdrm.Att_Wvf);

   memmove(&gdrm.Att_Ptf, &buf[off], sizeof(gdrm.Att_Ptf));
   off += sizeof(gdrm.Att_Ptf);

   memmove(&gdrm.H_Alt, &buf[off], sizeof(gdrm.H_Alt));
   off += sizeof(gdrm.H_Alt);

   memmove(&gdrm.H_Alt_SME, &buf[off], sizeof(gdrm.H_Alt_SME));
   off += sizeof(gdrm.H_Alt_SME);

   memmove(&gdrm.Nval_H_Alt, &buf[off], sizeof(gdrm.Nval_H_Alt));
   off += sizeof(gdrm.Nval_H_Alt);

   memmove(&gdrm.RMS_H_Alt, &buf[off], sizeof(gdrm.RMS_H_Alt));
   off += sizeof(gdrm.RMS_H_Alt);

   memmove(&gdrm.Net_Instr_R_Corr_K, &buf[off], sizeof(gdrm.Net_Instr_R_Corr_K));
   off += sizeof(gdrm.Net_Instr_R_Corr_K);

   memmove(&gdrm.Net_Instr_R_Corr_C, &buf[off], sizeof(gdrm.Net_Instr_R_Corr_C));
   off += sizeof(gdrm.Net_Instr_R_Corr_C);

   memmove(&gdrm.Cg_Range_Corr, &buf[off], sizeof(gdrm.Cg_Range_Corr));
   off += sizeof(gdrm.Cg_Range_Corr);

   memmove(&gdrm.Range_Deriv, &buf[off], sizeof(gdrm.Range_Deriv));
   off += sizeof(gdrm.Range_Deriv);

   memmove(&gdrm.RMS_Range_Deriv, &buf[off], sizeof(gdrm.RMS_Range_Deriv));
   off += sizeof(gdrm.RMS_Range_Deriv);

   memmove(&gdrm.Dry_Corr, &buf[off], sizeof(gdrm.Dry_Corr));
   off += sizeof(gdrm.Dry_Corr);

   memmove(&gdrm.Dry1_Corr, &buf[off], sizeof(gdrm.Dry1_Corr));
   off += sizeof(gdrm.Dry1_Corr);

   memmove(&gdrm.Dry2_Corr, &buf[off], sizeof(gdrm.Dry2_Corr));
   off += sizeof(gdrm.Dry2_Corr);

   memmove(&gdrm.INV_BAR, &buf[off], sizeof(gdrm.INV_BAR));
   off += sizeof(gdrm.INV_BAR);

   memmove(&gdrm.Wet_Corr, &buf[off], sizeof(gdrm.Wet_Corr));
   off += sizeof(gdrm.Wet_Corr);

   memmove(&gdrm.Wet1_Corr, &buf[off], sizeof(gdrm.Wet1_Corr));
   off += sizeof(gdrm.Wet1_Corr);

   memmove(&gdrm.Wet2_Corr, &buf[off], sizeof(gdrm.Wet2_Corr));
   off += sizeof(gdrm.Wet2_Corr);

   memmove(&gdrm.Wet_H_Rad, &buf[off], sizeof(gdrm.Wet_H_Rad));
   off += sizeof(gdrm.Wet_H_Rad);

   memmove(&gdrm.Iono_Corr, &buf[off], sizeof(gdrm.Iono_Corr));
   off += sizeof(gdrm.Iono_Corr);

   memmove(&gdrm.Iono_Dor, &buf[off], sizeof(gdrm.Iono_Dor));
   off += sizeof(gdrm.Iono_Dor);

   memmove(&gdrm.Iono_Ben, &buf[off], sizeof(gdrm.Iono_Ben));
   off += sizeof(gdrm.Iono_Ben);

   memmove(&gdrm.SWH_K, &buf[off], sizeof(gdrm.SWH_K));
   off += sizeof(gdrm.SWH_K);

   memmove(&gdrm.SWH_C, &buf[off], sizeof(gdrm.SWH_C));
   off += sizeof(gdrm.SWH_C);

   memmove(&gdrm.SWH_RMS_K, &buf[off], sizeof(gdrm.SWH_RMS_K));
   off += sizeof(gdrm.SWH_RMS_K);

   memmove(&gdrm.SWH_RMS_C, &buf[off], sizeof(gdrm.SWH_RMS_C));
   off += sizeof(gdrm.SWH_RMS_C);

   memmove(&gdrm.SWH_Pts_Avg, &buf[off], sizeof(gdrm.SWH_Pts_Avg));
   off += sizeof(gdrm.SWH_Pts_Avg);

   memmove(&gdrm.Net_Instr_SWH_Corr_K,
      &buf[off], sizeof(gdrm.Net_Instr_SWH_Corr_K));
   off += sizeof(gdrm.Net_Instr_SWH_Corr_K);

   memmove(&gdrm.Net_Instr_SWH_Corr_C,
      &buf[off], sizeof(gdrm.Net_Instr_SWH_Corr_C));
   off += sizeof(gdrm.Net_Instr_SWH_Corr_C);

   memmove(&gdrm.DR_SWH_Att_K, &buf[off], sizeof(gdrm.DR_SWH_Att_K));
   off += sizeof(gdrm.DR_SWH_Att_K);

   memmove(&gdrm.DR_SWH_Att_C, &buf[off], sizeof(gdrm.DR_SWH_Att_C));
   off += sizeof(gdrm.DR_SWH_Att_C);

   memmove(&gdrm.EMB_Gaspar,
      &buf[off], sizeof(gdrm.EMB_Gaspar));
   off += sizeof(gdrm.EMB_Gaspar);

   memmove(&gdrm.EMB_Walsh,
      &buf[off], sizeof(gdrm.EMB_Walsh));
   off += sizeof(gdrm.EMB_Walsh);

   memmove(&gdrm.Sigma0_K, &buf[off], sizeof(gdrm.Sigma0_K));
   off += sizeof(gdrm.Sigma0_K);

   memmove(&gdrm.Sigma0_C, &buf[off], sizeof(gdrm.Sigma0_C));
   off += sizeof(gdrm.Sigma0_C);

   memmove(&gdrm.AGC_K, &buf[off], sizeof(gdrm.AGC_K));
   off += sizeof(gdrm.AGC_K);

   memmove(&gdrm.AGC_C, &buf[off], sizeof(gdrm.AGC_C));
   off += sizeof(gdrm.AGC_C);

   memmove(&gdrm.AGC_RMS_K, &buf[off], sizeof(gdrm.AGC_RMS_K));
   off += sizeof(gdrm.AGC_RMS_K);

   memmove(&gdrm.AGC_RMS_C, &buf[off], sizeof(gdrm.AGC_RMS_C));
   off += sizeof(gdrm.AGC_RMS_C);

   memmove(&gdrm.Atm_Att_Sig0_Corr, &buf[off], sizeof(gdrm.Atm_Att_Sig0_Corr));
   off += sizeof(gdrm.Atm_Att_Sig0_Corr);

   memmove(&gdrm.Net_Instr_Sig0_Corr, &buf[off], sizeof(gdrm.Net_Instr_Sig0_Corr));
   off += sizeof(gdrm.Net_Instr_Sig0_Corr);

   memmove(&gdrm.Net_Instr_AGC_Corr_K,
      &buf[off], sizeof(gdrm.Net_Instr_AGC_Corr_K));
   off += sizeof(gdrm.Net_Instr_AGC_Corr_K);

   memmove(&gdrm.Net_Instr_AGC_Corr_C,
      &buf[off], sizeof(gdrm.Net_Instr_AGC_Corr_C));
   off += sizeof(gdrm.Net_Instr_AGC_Corr_C);

   memmove(&gdrm.AGC_Pts_Avg, &buf[off], sizeof(gdrm.AGC_Pts_Avg));
   off += sizeof(gdrm.AGC_Pts_Avg);

   memmove(&gdrm.H_MSS, &buf[off], sizeof(gdrm.H_MSS));
   off += sizeof(gdrm.H_MSS);

   memmove(&gdrm.H_Geo, &buf[off], sizeof(gdrm.H_Geo));
   off += sizeof(gdrm.H_Geo);

   memmove(&gdrm.H_EOT_CSR, &buf[off], sizeof(gdrm.H_EOT_CSR));
   off += sizeof(gdrm.H_EOT_CSR);

   memmove(&gdrm.H_EOT_FES, &buf[off], sizeof(gdrm.H_EOT_FES));
   off += sizeof(gdrm.H_EOT_FES);

   memmove(&gdrm.H_LT, &buf[off], sizeof(gdrm.H_LT));
   off += sizeof(gdrm.H_LT);

   memmove(&gdrm.H_Set, &buf[off], sizeof(gdrm.H_Set));
   off += sizeof(gdrm.H_Set);

   memmove(&gdrm.H_Pol, &buf[off], sizeof(gdrm.H_Pol));
   off += sizeof(gdrm.H_Pol);

   memmove(&gdrm.Wind_Sp, &buf[off], sizeof(gdrm.Wind_Sp));
   off += sizeof(gdrm.Wind_Sp);

   memmove(&gdrm.H_Ocs, &buf[off], sizeof(gdrm.H_Ocs));
   off += sizeof(gdrm.H_Ocs);

   memmove(&gdrm.Tb_18, &buf[off], sizeof(gdrm.Tb_18));
   off += sizeof(gdrm.Tb_18);

   memmove(&gdrm.Tb_21, &buf[off], sizeof(gdrm.Tb_21));
   off += sizeof(gdrm.Tb_21);

   memmove(&gdrm.Tb_37, &buf[off], sizeof(gdrm.Tb_37));
   off += sizeof(gdrm.Tb_37);

   memmove(&gdrm.ALTON, &buf[off], sizeof(gdrm.ALTON));
   off += sizeof(gdrm.ALTON);

   memmove(&gdrm.Instr_State_Topex,
      &buf[off], sizeof(gdrm.Instr_State_Topex));
   off += sizeof(gdrm.Instr_State_Topex);

   memmove(&gdrm.Instr_State_TMR,
      &buf[off], sizeof(gdrm.Instr_State_TMR));
   off += sizeof(gdrm.Instr_State_TMR);

   memmove(&gdrm.Instr_State_DORIS,
      &buf[off], sizeof(gdrm.Instr_State_DORIS));
   off += sizeof(gdrm.Instr_State_DORIS);

   memmove(&gdrm.IMANV, &buf[off], sizeof(gdrm.IMANV));
   off += sizeof(gdrm.IMANV);

   memmove(&gdrm.Lat_Err, &buf[off], sizeof(gdrm.Lat_Err));
   off += sizeof(gdrm.Lat_Err);

   memmove(&gdrm.Lon_Err, &buf[off], sizeof(gdrm.Lon_Err));
   off += sizeof(gdrm.Lon_Err);

   memmove(&gdrm.Val_Att_Ptf, &buf[off], sizeof(gdrm.Val_Att_Ptf));
   off += sizeof(gdrm.Val_Att_Ptf);

   memmove(&gdrm.Current_Mode_1,
      &buf[off], sizeof(gdrm.Current_Mode_1));
   off += sizeof(gdrm.Current_Mode_1);

   memmove(&gdrm.Current_Mode_2,
      &buf[off], sizeof(gdrm.Current_Mode_2));
   off += sizeof(gdrm.Current_Mode_2);

   memmove(&gdrm.Gate_Index, &buf[off], sizeof(gdrm.Gate_Index));
   off += sizeof(gdrm.Gate_Index);

   memmove(&gdrm.Ind_Pha, &buf[off], sizeof(gdrm.Ind_Pha));
   off += sizeof(gdrm.Ind_Pha);

   memmove(&gdrm.Rang_SME, &buf[off], sizeof(gdrm.Rang_SME));
   off += sizeof(gdrm.Rang_SME);

   memmove(&gdrm.Alt_Bad_1, &buf[off], sizeof(gdrm.Alt_Bad_1));
   off += sizeof(gdrm.Alt_Bad_1);

   memmove(&gdrm.Alt_Bad_2, &buf[off], sizeof(gdrm.Alt_Bad_2));
   off += sizeof(gdrm.Alt_Bad_2);

   memmove(&gdrm.Fl_Att, &buf[off], sizeof(gdrm.Fl_Att));
   off += sizeof(gdrm.Fl_Att);

   memmove(&gdrm.Dry_Err, &buf[off], sizeof(gdrm.Dry_Err));
   off += sizeof(gdrm.Dry_Err);

   memmove(&gdrm.Dry1_Err, &buf[off], sizeof(gdrm.Dry1_Err));
   off += sizeof(gdrm.Dry1_Err);

   memmove(&gdrm.Dry2_Err, &buf[off], sizeof(gdrm.Dry2_Err));
   off += sizeof(gdrm.Dry2_Err);

   memmove(&gdrm.Wet_Flag, &buf[off], sizeof(gdrm.Wet_Flag));
   off += sizeof(gdrm.Wet_Flag);

   memmove(&gdrm.Wet_H_Err, &buf[off], sizeof(gdrm.Wet_H_Err));
   off += sizeof(gdrm.Wet_H_Err);

   memmove(&gdrm.Iono_Bad, &buf[off], sizeof(gdrm.Iono_Bad));
   off += sizeof(gdrm.Iono_Bad);

   memmove(&gdrm.Iono_Dor_Bad, &buf[off], sizeof(gdrm.Iono_Dor_Bad));
   off += sizeof(gdrm.Iono_Dor_Bad);

   memmove(&gdrm.Geo_Bad_1, &buf[off], sizeof(gdrm.Geo_Bad_1));
   off += sizeof(gdrm.Geo_Bad_1);

   memmove(&gdrm.Geo_Bad_2, &buf[off], sizeof(gdrm.Geo_Bad_2));
   off += sizeof(gdrm.Geo_Bad_2);

   memmove(&gdrm.TMR_Bad, &buf[off], sizeof(gdrm.TMR_Bad));
   off += sizeof(gdrm.TMR_Bad);

   memmove(&gdrm.Ind_RTK, &buf[off], sizeof(gdrm.Ind_RTK));
   off += sizeof(gdrm.Ind_RTK);

   off++;
}
//---------------------------------------------------------------------------
/*
#
#               Преобразование времени к формату SQL
#
*/
AnsiString TopexThread::DATETIMEASTIMATION(int days,int ms,int mcs)
{
TDateTime rdt;
double topex_dt;
AnsiString Out;


        topex_dt=days+ms/86400000.+mcs/86400000000.;

        rdt=dt1958+topex_dt;

        NewFormat="yyyy/mm/dd";
        NewSeparator='-';

        OldLFormat=LongDateFormat;                  //Запоминаем формат даты
        OldSFormat=ShortDateFormat;
        OldSeparator=DateSeparator;

        LongDateFormat=NewFormat;
        ShortDateFormat=NewFormat;
        DateSeparator=NewSeparator;

        DATE=rdt.DateString();
        TIME=rdt.TimeString();
        Out=rdt.DateTimeString();

        LongDateFormat=OldLFormat;
        ShortDateFormat=OldSFormat;
        DateSeparator=OldSeparator;

return (Out);
}
//---------------------------------------------------------------------------
BOOL TopexThread::GETDATETIMEDVUCUR(AnsiString String)
{
AnsiString Buffer="\0";
AnsiString Time="\0";
int hour=0,min=0,sec=0;
//unsigned long msec=0;

        try
        {
                Buffer=String.SubString(1,String.Pos("-")-1);
                DVUCurYear=StrToInt(Buffer);

                String=String.SubString(String.Pos("-")+1,String.Length());
                Buffer=String.SubString(1,String.Pos("T")-1);
                DVUCurDay=StrToInt(Buffer);

                Time=String.SubString(String.Pos("T")+1,String.Length());

                Buffer=Time.SubString(1,Time.Pos(":")-1);
                hour=StrToInt(Buffer);
                Time=Time.SubString(Time.Pos(":")+1,Time.Length());

                Buffer=Time.SubString(1,Time.Pos(":")-1);
                min=StrToInt(Buffer);
                Time=Time.SubString(Time.Pos(":")+1,Time.Length());

                Buffer=Time.SubString(1,Time.Pos(".")-1);
                sec=StrToInt(Buffer);
                Time=Time.SubString(Time.Pos(".")+1,Time.Length());

//                msec=StrToInt(Time);

                DVUCurTimeSec=hour*3600+min*60+sec;
        }
        catch (...)
        {
                return false;
        }
return true;
}

//---------------------------------------------------------------------------


