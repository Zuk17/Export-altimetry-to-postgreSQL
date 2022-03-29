//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "ThreadJason.h"

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
//      void __fastcall Unit1::UpdateCaption()
//      {
//        Form1->Caption = "Updated in a thread";
//      }
//---------------------------------------------------------------------------
__fastcall JasonThread::JasonThread(bool CreateSuspended)
        : TThread(CreateSuspended)
{
        FreeOnTerminate=true;
}
//---------------------------------------------------------------------------
void __fastcall JasonThread::Execute()
{
Count_Errors=0;

if_header=new jason_igdr_header;
//j1gdr=new gdr_j1_header;
        List_Debug=new TStringList;
        List_Debug->Clear();

        DirList=new TStringList;
        ListOut=new TStringList;
        ListOut->Clear();
        dt1958=TDateTime(1958,01,01);
        if (CreatingTleDB(Alt_start->Button_tle->Caption))
        {
                ListOut->Clear();
                Alt_start->Status_help->Items->Add("Start scanning path to GDR data...");
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
                                CalculatingGDR(ListOut->Strings[i]);
                        }
                        catch(...)
                        {
                                Alt_start->Status_help->Items->Add(Buffer_tmp.sprintf("Cann`t to calculate MGDR at cycle %s",ListOut->Strings[i]));
                        }
                }
                Alt_start->Status_help->Items->Add("Calculation finished. Data saved at PostgreSQL DB.");
                Alt_start->Status_help->Items->Add(IntToStr(Count_Errors));


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
//        List_Debug->SaveToFile("d:/alt_debug.txt");
        delete List_Debug;
        delete DirList;
        delete ListOut;
delete if_header;
//delete j1gdr;
        return;

}
//---------------------------------------------------------------------------
void JasonThread::CalculatingGDR (AnsiString Path_to_DATA)
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
                unsigned char *buf;
                int offset=0;
                long  start_of_data_offset;
                int NumRec=0;
                buf=new unsigned char[J1GDR_RECSIZE];

//int in=0,i=0,j=0,counter=0,k=0;
//AnsiString stDateTime;
//                int nbytes, status;
                int   nrec;//, numrecs, datarecs, start, end;
//TDateTime cd;

        //Считывание и обработка файла
                for (int i=0;i<DirList->Count;i++,Alt_start->Progress_mgdr_dir->Position++)
                {
 //                 filelen=0;

                  bPassHdr=GETPASSHDR(DirList->Strings[i]);

                  if (bPassHdr)
                  {
                        if ((in = FileOpen(DirList->Strings[i], fmOpenRead)) < 0)
                                return;

//                        FILEFF=DirList->Strings[i];

                        start_of_data_offset = TOTAL_J1GDR_HEADER_CHARS;


                        if (NumRec<=0)
                        {
                                offset = FileSeek(in, 0, SEEK_END);
                                NumRec = (offset - start_of_data_offset) / J1GDR_RECSIZE;
                        }



                       Alt_start->Progress_mgdr_file->Min=0;
                       Alt_start->Progress_mgdr_file->Max=NumRec-1;

                       for (nrec = 0; nrec < NumRec; nrec++,Alt_start->Progress_mgdr_file->Position++)
                        {

                                offset = start_of_data_offset + J1GDR_RECSIZE*(nrec);
                                FileSeek(in, offset, SEEK_SET);

                                if((FileRead(in,buf, J1GDR_RECSIZE)) != 0)
                                  if(decode(buf) != 0)
                                        AltCount();
                        }
                        Alt_start->Progress_mgdr_file->Position=0;
                        Alt_start->Refresh();
                  }
                }
                        delete buf;
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
BOOL JasonThread::CreatingTleDB (AnsiString Path_to_Tle)
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
  //Создание массива данных TLE для Jason1
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
  //Поиск строк для Jason1
                 while (fgets(msg, 256, iin)!=NULL)
                       if (strncmp(msg,"1 26997U",8)==0) break;
  //Занесение строк в массив
                 if (strncmp("1 26997U",msg,8)==0)
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
BOOL JasonThread::AltCount(void)
{
AnsiString Buffer;
AnsiString BC;
int k=0;
long h_orb[20],h_range[20];
int j=0;
ofstream fout;


/*-----------------------------------------------*/


     latitude=(double)j1gdr.latitude/1000000.;
     if (j1gdr.longitude/1000000.>=180.0)
          longitude=(double)((double)j1gdr.longitude/1000000-360);
      else
          longitude=(double)j1gdr.longitude/1000000.;

     if (((latitude>45) && (latitude<54) && (longitude>117.6) && (longitude<140)) ||
     	 	((latitude>65) && (latitude<68) && (longitude>65) && (longitude<68)))
     {

//        int H_REF=StrToInt(if_header->Range_Offset)*1000*10000; //Пиздец!я хуею дороая редакция!!!

// Orbit
//        if (j1gdr.altitude!=4294967295 &&(j1gdr.altitude>=300000000&&j1gdr.altitude<=700000000)) // altitude is good
        {
		H_SAT=long(j1gdr.altitude);

                for (k=0;k<20;k++)
                        h_orb[k]=H_SAT+long(j1gdr.alt_hi_rate[k]);
        }
//        else {H_SAT=4294967295;Count_Errors++;return true;}

//Altimeter range
//        if (j1gdr.range_ku!=4294967295 &&(j1gdr.range_ku>=300000000&&j1gdr.range_ku<=700000000)) // altitude is good
        {
                H_Alt=long(j1gdr.range_ku);

                for (k=0;k<20;k++)
                        h_range[k]=H_Alt+long(j1gdr.range_hi_rate_ku[k]);
        }
//       else {H_Alt=4294967295;Count_Errors++;return true;}

//Ionosphere correction
        if (j1gdr.iono_corr_alt_ku!=32767 ) // iono_corr is good
                Iono_Cor=j1gdr.iono_corr_alt_ku;
        else Iono_Cor=32767;

        if (j1gdr.iono_corr_doris_ku<=0&&j1gdr.iono_corr_doris_ku>=-1000)
                Iono_Dor=j1gdr.iono_corr_doris_ku;
        else Iono_Dor=32767;

// Dry correction
        if (j1gdr.model_dry_tropo_corr!=32767 &&(j1gdr.model_dry_tropo_corr>=-25000&&j1gdr.model_dry_tropo_corr<=-19000)) // dry_cor is good
		Dry_Cor=j1gdr.model_dry_tropo_corr;
        else Dry_Cor=32767;
// Wet correction
        if (j1gdr.model_wet_tropo_corr!=32767 &&(j1gdr.model_wet_tropo_corr>=-5000&&j1gdr.model_wet_tropo_corr< -10)) // wet_corr is good
                Wet_Cor=j1gdr.model_wet_tropo_corr;
        else Wet_Cor=32767;

        Wet1_Cor=32767;
        Wet2_Cor=32767;

        if (j1gdr.rad_wet_tropo_corr!=32767 &&(j1gdr.rad_wet_tropo_corr>=-5000&&j1gdr.rad_wet_tropo_corr< -10)) // wet_corr is good
                Wet_H=j1gdr.rad_wet_tropo_corr;
        else Wet_H=32767;


// Solid Earth Tide
        if (j1gdr.solid_earth_tide!=32767 &&(j1gdr.solid_earth_tide>=-10000&&j1gdr.solid_earth_tide<=10000)) // solid earth tide is good
                H_SET=j1gdr.solid_earth_tide;
        else H_SET=32767;

// Geocentric Pole Tide
        if (j1gdr.pole_tide!=32767 &&(j1gdr.pole_tide>=-1000&&j1gdr.pole_tide<=1000)) // pole_tide is good
                H_POL=j1gdr.pole_tide;
        else H_POL=32767;

//Geoid
        if (j1gdr.geoid!=2147483647 &&(j1gdr.geoid>=-1500000&&j1gdr.geoid<=1500000)) // geoid is good
                H_GEO=j1gdr.geoid;
        else H_GEO=2147483647;
// Inverse Barometer Effect
        if (j1gdr.inv_bar_corr!=32767) // inv_bar is good
                INV_BAR=j1gdr.inv_bar_corr;
        else INV_BAR=32767;


//-------------------------------->

//        Buffer.sprintf("%s,%.3f,%.3f,%.2f,%s,%s",DATETIMEASTIMATION(j1gdr.time_day,j1gdr.time_sec,j1gdr.time_microsec),longitude,latitude,(H_SAT-H_Alt-Wet_Cor-Dry_Cor-Iono_Cor-ssb_ku-mss-INV_BAR-high_freq_wind_resp-ocean_tide-H_SET-H_POL)/100.,if_header->Cycle_Number,if_header->Pass_Number); //перевод в метры
        Buffer.sprintf("insert into alt_data_1hz values ('J1',%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,cast('%s' as timestamp),GeomFromText('POINT(%.3f %.3f)',32652),%s,%s)",longitude,latitude,(H_SAT-H_Alt)/100.,Iono_Cor/100.,Iono_Dor/100.,Dry_Cor/100.,Wet_Cor/100.,Wet1_Cor/100.,Wet2_Cor/100.,Wet_H/100.,H_GEO/100.,H_SET/1000.,H_POL/100.,INV_BAR/100.,DATETIMEASTIMATION(j1gdr.time_day,j1gdr.time_sec,j1gdr.time_microsec),longitude,latitude,if_header->Cycle_Number,if_header->Pass_Number); //перевод в метры
//        List_Debug->Add(Buffer);

        Alt_start->res = Alt_start->PQexec(Alt_start->conn, Buffer.c_str());
        if (Alt_start->PQresultStatus(Alt_start->res) != PGRES_COMMAND_OK)
             MessageDlg(Alt_start->PQerrorMessage(Alt_start->conn),mtError,TMsgDlgButtons() << mbOK,0);
        Alt_start->PQclear(Alt_start->res);


//Calculating Time
int year=1958;
int day=1;
int delta=j1gdr.time_day;
double time_tmp=0;

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
        if ((TleData_List[j].day<delta) || ((TleData_List[j].day==delta)&&(TleData_List[j].time<(j1gdr.time_sec+j1gdr.time_microsec/1000000))))
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


           time_tmp=int(j1gdr.time_microsec)-StrToInt(if_header->Time_Shift_Mid_Frame)-StrToInt(if_header->Time_Shift_Interval);
           time_tmp=double(time_tmp)/1000000;
           time_tmp+=double(j1gdr.time_sec);

            for (k=0;k<20;k++)
           {
//              if((h_orb[k]-h_range[k]<100000)&&(h_orb[k]-h_range[k]>-100000)){
                time_tmp+=double(StrToInt(if_header->Time_Shift_Interval))/1000000.;
                //время в минутах с эпохи!!! функция возвращает координату в eci
                orbit.getPosition((double(time_tmp-Tle_work.time)/60+double(delta-Tle_work.day)*1440), &eci);
                //преобразует координату
                FreshCoord=eci.toGeo();
//                alt_land[i]=h_orb[i]-(h_range[i]+Dry_Cor+Iono_Cor)-H_GEO-H_SET;

                Buffer.sprintf("insert into alt_data_10hz values ('J1',%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,cast('%s' as timestamp),GeomFromText('POINT(%.3f %.3f)',32652),%s,%s)",FreshCoord.m_Lon*180/PI,FreshCoord.m_Lat*180/PI,(h_orb[k]-h_range[k])/100.,Iono_Cor/100.,Iono_Dor/100.,Dry_Cor/100.,Wet_Cor/100.,Wet1_Cor/100.,Wet2_Cor/100.,Wet_H/100.,H_GEO/100.,H_SET/100.,H_POL/100.,INV_BAR/100.,DATETIMEASTIMATION(j1gdr.time_day,int(time_tmp),0),FreshCoord.m_Lon*180/PI,FreshCoord.m_Lat*180/PI,if_header->Cycle_Number,if_header->Pass_Number); //перевод в сантиметры
                Alt_start->res = Alt_start->PQexec(Alt_start->conn, Buffer.c_str());
                if (Alt_start->PQresultStatus(Alt_start->res) != PGRES_COMMAND_OK)
                     MessageDlg(Alt_start->PQerrorMessage(Alt_start->conn),mtError,TMsgDlgButtons() << mbOK,0);
                Alt_start->PQclear(Alt_start->res);
//                }else Count_Errors++;
           }
return true;
}
else return false;
}


//------------------------------------------------------------------------------
bool JasonThread::decode(unsigned char *buf)
{
   int i, off;

   off = 0;

   try
   {

   /* TIME TAG */
   memmove(&j1gdr.time_day, &buf[off], sizeof(j1gdr.time_day));
   off += sizeof(j1gdr.time_day);

   memmove(&j1gdr.time_sec, &buf[off], sizeof(j1gdr.time_sec));
   off += sizeof(j1gdr.time_sec);

   memmove(&j1gdr.time_microsec, &buf[off], sizeof(j1gdr.time_microsec));
   off += sizeof(j1gdr.time_microsec);

   /* LOCATION AND SURFACE TYPE */
   memmove(&j1gdr.latitude, &buf[off], sizeof(j1gdr.latitude));
   off += sizeof(j1gdr.latitude);

   memmove(&j1gdr.longitude, &buf[off], sizeof(j1gdr.longitude));
   off += sizeof(j1gdr.longitude);

   memmove(&j1gdr.surface_type, &buf[off], sizeof(j1gdr.surface_type));
   off += sizeof(j1gdr.surface_type);

   memmove(&j1gdr.alt_echo_type, &buf[off], sizeof(j1gdr.alt_echo_type));
   off += sizeof(j1gdr.alt_echo_type);

   memmove(&j1gdr.rad_surf_type, &buf[off], sizeof(j1gdr.rad_surf_type));
   off += sizeof(j1gdr.rad_surf_type);

   /* QUALITY INFORMATION AND SENSORS STATUS */
   memmove(&j1gdr.qual_1hz_alt_data, &buf[off], sizeof(j1gdr.qual_1hz_alt_data));
   off += sizeof(j1gdr.qual_1hz_alt_data);

   memmove(&j1gdr.qual_1hz_alt_instr_corr, &buf[off], sizeof(j1gdr.qual_1hz_alt_instr_corr));
   off += sizeof(j1gdr.qual_1hz_alt_instr_corr);

   memmove(&j1gdr.qual_1hz_rad_data, &buf[off], sizeof(j1gdr.qual_1hz_rad_data));
   off += sizeof(j1gdr.qual_1hz_rad_data);

   memmove(&j1gdr.alt_state_flag, &buf[off], sizeof(j1gdr.alt_state_flag));
   off += sizeof(j1gdr.alt_state_flag);

   memmove(&j1gdr.rad_state_flag, &buf[off], sizeof(j1gdr.rad_state_flag));
   off += sizeof(j1gdr.rad_state_flag);

   memmove(&j1gdr.orb_state_flag, &buf[off], sizeof(j1gdr.orb_state_flag));
   off += sizeof(j1gdr.orb_state_flag);

   memmove(&j1gdr.qual_spare, &buf[off], sizeof(j1gdr.qual_spare));
   off += sizeof(j1gdr.qual_spare);

   /* ORBIT */
   memmove(&j1gdr.altitude, &buf[off], sizeof(j1gdr.altitude));
   off += sizeof(j1gdr.altitude);

   memmove(&j1gdr.alt_hi_rate, &buf[off], sizeof(j1gdr.alt_hi_rate));
   off += sizeof(j1gdr.alt_hi_rate);

   memmove(&j1gdr.orb_alt_rate, &buf[off], sizeof(j1gdr.orb_alt_rate));
   off += sizeof(j1gdr.orb_alt_rate);

   memmove(&j1gdr.orb_spare, &buf[off], sizeof(j1gdr.orb_spare));
   off += sizeof(j1gdr.orb_spare);

   /* ALTIMETER RANGE */
   memmove(&j1gdr.range_ku, &buf[off], sizeof(j1gdr.range_ku));
   off += sizeof(j1gdr.range_ku);

   memmove(&j1gdr.range_hi_rate_ku, &buf[off], sizeof(j1gdr.range_hi_rate_ku));
   off += sizeof(j1gdr.range_hi_rate_ku);

   memmove(&j1gdr.range_c, &buf[off], sizeof(j1gdr.range_c));
   off += sizeof(j1gdr.range_c);

   memmove(&j1gdr.range_hi_rate_c, &buf[off], sizeof(j1gdr.range_hi_rate_c));
   off += sizeof(j1gdr.range_hi_rate_c);

   memmove(&j1gdr.range_rms_ku, &buf[off], sizeof(j1gdr.range_rms_ku));
   off += sizeof(j1gdr.range_rms_ku);

   memmove(&j1gdr.range_rms_c, &buf[off], sizeof(j1gdr.range_rms_c));
   off += sizeof(j1gdr.range_rms_c);

   memmove(&j1gdr.range_numval_ku, &buf[off], sizeof(j1gdr.range_numval_ku));
   off += sizeof(j1gdr.range_numval_ku);

   memmove(&j1gdr.range_numval_c, &buf[off], sizeof(j1gdr.range_numval_c));
   off += sizeof(j1gdr.range_numval_c);

   memmove(&j1gdr.alt_spare, &buf[off], sizeof(j1gdr.alt_spare));
   off += sizeof(j1gdr.alt_spare);

   memmove(&j1gdr.range_mapvalpts_ku, &buf[off], sizeof(j1gdr.range_mapvalpts_ku));
   off += sizeof(j1gdr.range_mapvalpts_ku);

   memmove(&j1gdr.range_mapvalpts_c, &buf[off], sizeof(j1gdr.range_mapvalpts_c));
   off += sizeof(j1gdr.range_mapvalpts_c);

   /* ALTIMETER RANGE CORRECTIONS */
   memmove(&j1gdr.net_instr_corr_ku, &buf[off], sizeof(j1gdr.net_instr_corr_ku));
   off += sizeof(j1gdr.net_instr_corr_ku);

   memmove(&j1gdr.net_instr_corr_c, &buf[off], sizeof(j1gdr.net_instr_corr_c));
   off += sizeof(j1gdr.net_instr_corr_c);

   memmove(&j1gdr.model_dry_tropo_corr, &buf[off], sizeof(j1gdr.model_dry_tropo_corr));
   off += sizeof(j1gdr.model_dry_tropo_corr);

   memmove(&j1gdr.model_wet_tropo_corr, &buf[off], sizeof(j1gdr.model_wet_tropo_corr));
   off += sizeof(j1gdr.model_wet_tropo_corr);

   memmove(&j1gdr.rad_wet_tropo_corr, &buf[off], sizeof(j1gdr.rad_wet_tropo_corr));
   off += sizeof(j1gdr.rad_wet_tropo_corr);

   memmove(&j1gdr.iono_corr_alt_ku, &buf[off], sizeof(j1gdr.iono_corr_alt_ku));
   off += sizeof(j1gdr.iono_corr_alt_ku);

   memmove(&j1gdr.iono_corr_doris_ku, &buf[off], sizeof(j1gdr.iono_corr_doris_ku));
   off += sizeof(j1gdr.iono_corr_doris_ku);

   memmove(&j1gdr.sea_state_bias_ku, &buf[off], sizeof(j1gdr.sea_state_bias_ku));
   off += sizeof(j1gdr.sea_state_bias_ku);

   memmove(&j1gdr.sea_state_bias_c, &buf[off], sizeof(j1gdr.sea_state_bias_c));
   off += sizeof(j1gdr.sea_state_bias_c);

   memmove(&j1gdr.sea_state_bias_comp, &buf[off], sizeof(j1gdr.sea_state_bias_comp));
   off += sizeof(j1gdr.sea_state_bias_comp);

   /* SIGNIFICANT WAVEHEIGHT */
   memmove(&j1gdr.swh_ku, &buf[off], sizeof(j1gdr.swh_ku));
   off += sizeof(j1gdr.swh_ku);

   memmove(&j1gdr.swh_c, &buf[off], sizeof(j1gdr.swh_c));
   off += sizeof(j1gdr.swh_c);

   memmove(&j1gdr.swh_rms_ku, &buf[off], sizeof(j1gdr.swh_rms_ku));
   off += sizeof(j1gdr.swh_rms_ku);

   memmove(&j1gdr.swh_rms_c, &buf[off], sizeof(j1gdr.swh_rms_c));
   off += sizeof(j1gdr.swh_rms_c);

   memmove(&j1gdr.swh_numval_ku, &buf[off], sizeof(j1gdr.swh_numval_ku));
   off += sizeof(j1gdr.swh_numval_ku);

   memmove(&j1gdr.swh_numval_c, &buf[off], sizeof(j1gdr.swh_numval_c));
   off += sizeof(j1gdr.swh_numval_c);

   /* SIGNIFICANT WAVEHEIGHT CORRECTIONS */
   memmove(&j1gdr.net_instr_corr_swh_ku, &buf[off], sizeof(j1gdr.net_instr_corr_swh_ku));
   off += sizeof(j1gdr.net_instr_corr_swh_ku);

   memmove(&j1gdr.net_instr_corr_swh_c, &buf[off], sizeof(j1gdr.net_instr_corr_swh_c));
   off += sizeof(j1gdr.net_instr_corr_swh_c);

   /* BACKSCATTER COEFFICIENT */
   memmove(&j1gdr.sig0_ku, &buf[off], sizeof(j1gdr.sig0_ku));
   off += sizeof(j1gdr.sig0_ku);

   memmove(&j1gdr.sig0_c, &buf[off], sizeof(j1gdr.sig0_c));
   off += sizeof(j1gdr.sig0_c);

   memmove(&j1gdr.sig0_rms_ku, &buf[off], sizeof(j1gdr.sig0_rms_ku));
   off += sizeof(j1gdr.sig0_rms_ku);

   memmove(&j1gdr.sig0_rms_c, &buf[off], sizeof(j1gdr.sig0_rms_c));
   off += sizeof(j1gdr.sig0_rms_c);

   memmove(&j1gdr.sig0_numval_ku, &buf[off], sizeof(j1gdr.sig0_numval_ku));
   off += sizeof(j1gdr.sig0_numval_ku);

   memmove(&j1gdr.sig0_numval_c, &buf[off], sizeof(j1gdr.sig0_numval_c));
   off += sizeof(j1gdr.sig0_numval_c);

   memmove(&j1gdr.agc_ku, &buf[off], sizeof(j1gdr.agc_ku));
   off += sizeof(j1gdr.agc_ku);

   memmove(&j1gdr.agc_c, &buf[off], sizeof(j1gdr.agc_c));
   off += sizeof(j1gdr.agc_c);

   memmove(&j1gdr.agc_rms_ku, &buf[off], sizeof(j1gdr.agc_rms_ku));
   off += sizeof(j1gdr.agc_rms_ku);

   memmove(&j1gdr.agc_rms_c, &buf[off], sizeof(j1gdr.agc_rms_c));
   off += sizeof(j1gdr.agc_rms_c);

   memmove(&j1gdr.agc_numval_ku, &buf[off], sizeof(j1gdr.agc_numval_ku));
   off += sizeof(j1gdr.agc_numval_ku);

   memmove(&j1gdr.agc_numval_c, &buf[off], sizeof(j1gdr.agc_numval_c));
   off += sizeof(j1gdr.agc_numval_c);

   /* BACKSCATTER COEFFICIENT CORRECTIONS */
   memmove(&j1gdr.net_instr_sig0_corr_ku, &buf[off], sizeof(j1gdr.net_instr_sig0_corr_ku));
   off += sizeof(j1gdr.net_instr_sig0_corr_ku);

   memmove(&j1gdr.net_instr_sig0_corr_c, &buf[off], sizeof(j1gdr.net_instr_sig0_corr_c));
   off += sizeof(j1gdr.net_instr_sig0_corr_c);

   memmove(&j1gdr.atmos_sig0_corr_ku, &buf[off], sizeof(j1gdr.atmos_sig0_corr_ku));
   off += sizeof(j1gdr.atmos_sig0_corr_ku);

   memmove(&j1gdr.atmos_sig0_corr_c, &buf[off], sizeof(j1gdr.atmos_sig0_corr_c));
   off += sizeof(j1gdr.atmos_sig0_corr_c);

   /* off NADIR ANGLE */
   memmove(&j1gdr.off_nadir_angle_ku_wvf, &buf[off], sizeof(j1gdr.off_nadir_angle_ku_wvf));
   off += sizeof(j1gdr.off_nadir_angle_ku_wvf);

   memmove(&j1gdr.off_nadir_angle_ptf, &buf[off], sizeof(j1gdr.off_nadir_angle_ptf));
   off += sizeof(j1gdr.off_nadir_angle_ptf);

   /* BRIGHTNESS TEMPERATURES */

   memmove(&j1gdr.tb_187, &buf[off], sizeof(j1gdr.tb_187));
   off += sizeof(j1gdr.tb_187);

   memmove(&j1gdr.tb_238, &buf[off], sizeof(j1gdr.tb_238));
   off += sizeof(j1gdr.tb_238);

   memmove(&j1gdr.tb_340, &buf[off], sizeof(j1gdr.tb_340));
   off += sizeof(j1gdr.tb_340);

   /* GEOPHYSICAL PARAMETERS */
   memmove(&j1gdr.mss, &buf[off], sizeof(j1gdr.mss));
   off += sizeof(j1gdr.mss);

   memmove(&j1gdr.mss_tp_along_trk, &buf[off], sizeof(j1gdr.mss_tp_along_trk));
   off += sizeof(j1gdr.mss_tp_along_trk);

   memmove(&j1gdr.geoid, &buf[off], sizeof(j1gdr.geoid));
   off += sizeof(j1gdr.geoid);

   memmove(&j1gdr.bathymetry, &buf[off], sizeof(j1gdr. bathymetry));
   off += sizeof(j1gdr. bathymetry);

   memmove(&j1gdr.inv_bar_corr, &buf[off], sizeof(j1gdr.inv_bar_corr));
   off += sizeof(j1gdr.inv_bar_corr);

   memmove(&j1gdr.hf_fluctuations_corr, &buf[off], sizeof(j1gdr.hf_fluctuations_corr));
   off += sizeof(j1gdr.hf_fluctuations_corr);

   memmove(&j1gdr.geo_spare, &buf[off], sizeof(j1gdr.geo_spare));
   off += sizeof(j1gdr.geo_spare);

   memmove(&j1gdr.ocean_tide_sol1, &buf[off], sizeof(j1gdr.ocean_tide_sol1));
   off += sizeof(j1gdr.ocean_tide_sol1);

   memmove(&j1gdr.ocean_tide_sol2, &buf[off], sizeof(j1gdr.ocean_tide_sol2));
   off += sizeof(j1gdr.ocean_tide_sol2);

   memmove(&j1gdr.ocean_tide_equil, &buf[off], sizeof(j1gdr.ocean_tide_equil));
   off += sizeof(j1gdr.ocean_tide_equil);

   memmove(&j1gdr.ocean_tide_non_equil, &buf[off], sizeof(j1gdr.ocean_tide_non_equil));
   off += sizeof(j1gdr.ocean_tide_non_equil);

   memmove(&j1gdr.load_tide_sol1, &buf[off], sizeof(j1gdr.load_tide_sol1));
   off += sizeof(j1gdr.load_tide_sol1);

   memmove(&j1gdr.load_tide_sol2, &buf[off], sizeof(j1gdr.load_tide_sol2));
   off += sizeof(j1gdr.load_tide_sol2);

   memmove(&j1gdr.solid_earth_tide, &buf[off], sizeof(j1gdr.solid_earth_tide));
   off += sizeof(j1gdr.solid_earth_tide);

   memmove(&j1gdr.pole_tide, &buf[off], sizeof(j1gdr.pole_tide));
   off += sizeof(j1gdr.pole_tide);

   /* ENVIRONMENTAL PARAMETERS */
   memmove(&j1gdr.wind_speed_model_u, &buf[off], sizeof(j1gdr.wind_speed_model_u));
   off += sizeof(j1gdr.wind_speed_model_u);

   memmove(&j1gdr.wind_speed_model_v, &buf[off], sizeof(j1gdr.wind_speed_model_v));
   off += sizeof(j1gdr.wind_speed_model_v);

   memmove(&j1gdr.wind_speed_alt, &buf[off], sizeof(j1gdr.wind_speed_alt));
   off += sizeof(j1gdr.wind_speed_alt);

   memmove(&j1gdr.wind_speed_rad, &buf[off], sizeof(j1gdr.wind_speed_rad));
   off += sizeof(j1gdr.wind_speed_rad);

   memmove(&j1gdr.rad_water_vapor, &buf[off], sizeof(j1gdr.rad_water_vapor));
   off += sizeof(j1gdr.rad_water_vapor);

   memmove(&j1gdr.rad_liquid_water, &buf[off], sizeof(j1gdr.rad_liquid_water));
   off += sizeof(j1gdr.rad_liquid_water);

   /* FLAGS */
   memmove(&j1gdr.ecmwf_meteo_map_avail, &buf[off], sizeof(j1gdr.ecmwf_meteo_map_avail));
   off += sizeof(j1gdr.ecmwf_meteo_map_avail);

   memmove(&j1gdr.tb_interp_flag, &buf[off], sizeof(j1gdr.tb_interp_flag));
   off += sizeof(j1gdr.tb_interp_flag);

   memmove(&j1gdr.rain_flag, &buf[off], sizeof(j1gdr.rain_flag));
   off += sizeof(j1gdr.rain_flag);

   memmove(&j1gdr.ice_flag, &buf[off], sizeof(j1gdr.ice_flag));
   off += sizeof(j1gdr.ice_flag);

   memmove(&j1gdr.interp_flag, &buf[off], sizeof(j1gdr.interp_flag));
   off += sizeof(j1gdr.interp_flag);

   memmove(&j1gdr.flag_spare, &buf[off], sizeof(j1gdr.flag_spare));
   off += sizeof(j1gdr.flag_spare);

   /* If your system does not use Unix byte order, bytes have to be swapped
      for 2 and 4 byte integers, since the byte order is reversed from Unix.
      Remove the comments around the swapbyte and recompile.
   */

   swapbyte((char *)&j1gdr.time_day, 0, 4);
   swapbyte((char *)&j1gdr.time_sec, 0, 4);
   swapbyte((char *)&j1gdr.time_microsec, 0, 4);

   swapbyte((char *)&j1gdr.latitude, 0, 4);
   swapbyte((char *)&j1gdr.longitude, 0, 4);

   swapbyte((char *)&j1gdr.altitude, 0, 4);
   for (i = 0; i < 20; i++) {
      swapbyte((char *)&j1gdr.alt_hi_rate[i], 0, 4);
   }
   swapbyte((char *)&j1gdr.orb_alt_rate, 0, 2);

   swapbyte((char *)&j1gdr.range_ku, 0, 4);
   for (i = 0; i < 20; i++) {
      swapbyte((char *)&j1gdr.range_hi_rate_ku[i], 0, 4);
   }
   swapbyte((char *)&j1gdr.range_c, 0, 4);
   for (i = 0; i < 20; i++) {
      swapbyte((char *)&j1gdr.range_hi_rate_c[i], 0, 4);
   }
   swapbyte((char *)&j1gdr.range_rms_ku, 0, 2);
   swapbyte((char *)&j1gdr.range_rms_c, 0, 2);
   for (i = 0; i < 2; i++) {
      swapbyte((char *)&j1gdr.alt_spare[i], 0, 2);
   }
   swapbyte((char *)&j1gdr.range_mapvalpts_ku, 0, 4);
   swapbyte((char *)&j1gdr.range_mapvalpts_c, 0, 4);

   swapbyte((char *)&j1gdr.net_instr_corr_ku, 0, 4);
   swapbyte((char *)&j1gdr.net_instr_corr_c, 0, 4);
   swapbyte((char *)&j1gdr.model_dry_tropo_corr, 0, 2);
   swapbyte((char *)&j1gdr.model_wet_tropo_corr, 0, 2);
   swapbyte((char *)&j1gdr.rad_wet_tropo_corr, 0, 2);
   swapbyte((char *)&j1gdr.iono_corr_alt_ku, 0, 2);
   swapbyte((char *)&j1gdr.iono_corr_doris_ku, 0, 2);
   swapbyte((char *)&j1gdr.sea_state_bias_ku, 0, 2);
   swapbyte((char *)&j1gdr.sea_state_bias_c, 0, 2);
   swapbyte((char *)&j1gdr.sea_state_bias_comp, 0, 2);

   swapbyte((char *)&j1gdr.swh_ku, 0, 2);
   swapbyte((char *)&j1gdr.swh_c, 0, 2);
   swapbyte((char *)&j1gdr.swh_rms_ku, 0, 2);
   swapbyte((char *)&j1gdr.swh_rms_c, 0, 2);

   swapbyte((char *)&j1gdr.net_instr_corr_swh_ku, 0, 2);
   swapbyte((char *)&j1gdr.net_instr_corr_swh_c, 0, 2);

   swapbyte((char *)&j1gdr.sig0_ku, 0, 2);
   swapbyte((char *)&j1gdr.sig0_c, 0, 2);
   swapbyte((char *)&j1gdr.sig0_rms_ku, 0, 2);
   swapbyte((char *)&j1gdr.sig0_rms_c, 0, 2);
   swapbyte((char *)&j1gdr.agc_ku, 0, 2);
   swapbyte((char *)&j1gdr.agc_c, 0, 2);
   swapbyte((char *)&j1gdr.agc_rms_ku, 0, 2);
   swapbyte((char *)&j1gdr.agc_rms_c, 0, 2);

   swapbyte((char *)&j1gdr.net_instr_sig0_corr_ku, 0, 2);
   swapbyte((char *)&j1gdr.net_instr_sig0_corr_c, 0, 2);
   swapbyte((char *)&j1gdr.atmos_sig0_corr_ku, 0, 2);
   swapbyte((char *)&j1gdr.atmos_sig0_corr_c, 0, 2);

   swapbyte((char *)&j1gdr.off_nadir_angle_ku_wvf, 0, 2);
   swapbyte((char *)&j1gdr.off_nadir_angle_ptf, 0, 2);

   swapbyte((char *)&j1gdr.tb_187, 0, 2);
   swapbyte((char *)&j1gdr.tb_238, 0, 2);
   swapbyte((char *)&j1gdr.tb_340, 0, 2);

   swapbyte((char *)&j1gdr.mss, 0, 4);
   swapbyte((char *)&j1gdr.mss_tp_along_trk, 0, 4);
   swapbyte((char *)&j1gdr.geoid, 0, 4);
   swapbyte((char *)&j1gdr.bathymetry, 0, 2);
   swapbyte((char *)&j1gdr.inv_bar_corr, 0, 2);
   swapbyte((char *)&j1gdr.hf_fluctuations_corr, 0, 2);
   swapbyte((char *)&j1gdr.ocean_tide_sol1, 0, 4);
   swapbyte((char *)&j1gdr.ocean_tide_sol2, 0, 4);
   swapbyte((char *)&j1gdr.ocean_tide_equil, 0, 2);
   swapbyte((char *)&j1gdr.ocean_tide_non_equil, 0, 2);
   swapbyte((char *)&j1gdr.load_tide_sol1, 0, 2);
   swapbyte((char *)&j1gdr.load_tide_sol2, 0, 2);
   swapbyte((char *)&j1gdr.solid_earth_tide, 0, 2);
   swapbyte((char *)&j1gdr.pole_tide, 0, 2);

   swapbyte((char *)&j1gdr.wind_speed_model_u, 0, 2);
   swapbyte((char *)&j1gdr.wind_speed_model_v, 0, 2);
   swapbyte((char *)&j1gdr.wind_speed_alt, 0, 2);
   swapbyte((char *)&j1gdr.wind_speed_rad, 0, 2);
   swapbyte((char *)&j1gdr.rad_water_vapor, 0, 2);
   swapbyte((char *)&j1gdr.rad_liquid_water, 0, 2);


   return true;
   }
   catch (...)
   {
        return false;
   }

}
//----------------------------------------------------------------------------
int JasonThread::swapbyte(char buf [], int start_byte, int num_bytes)
{
char temp;

   if (num_bytes == 2) {
      temp = buf[start_byte];
      buf[start_byte] = buf[start_byte + 1];
      buf[start_byte + 1] = temp;
      return (0);
   }
   else if (num_bytes == 4) {
      temp = buf[start_byte];
      buf[start_byte] = buf[start_byte + 3];
      buf[start_byte + 3] = temp;
      temp = buf[start_byte + 1];
      buf[start_byte + 1] = buf[start_byte + 2];
      buf[start_byte + 2] = temp;
      return (0);
   }
   else {
//      printf("swap_bytes: num_bytes not one of 2 or 4.\n");
      return(1);
   }
}
//---------------------------------------------------------------------------
BOOL JasonThread::GETPASSHDR (AnsiString FileName)
{
TStringList *Header=new TStringList;
AnsiString Buffer="\0";
unsigned short dd,mm,yy,hh,mo,ss,ms;

        try
        {
                Header->Clear();
                Header->LoadFromFile(FileName);

                if_header->Product_File_Name = Header->Strings[1].SubString(Header->Strings[1].Pos("=")+1,Header->Strings[1].Pos(";")-Header->Strings[1].Pos("=")-1).Trim();
                if_header->Producer_Agency_Name =Header->Strings[2].SubString(Header->Strings[2].Pos("=")+1,Header->Strings[2].Pos(";")-Header->Strings[2].Pos("=")-1).Trim();
                if_header->Processing_Center = Header->Strings[3].SubString(Header->Strings[3].Pos("=")+1,Header->Strings[3].Pos(";")-Header->Strings[3].Pos("=")-1).Trim();
                if_header->File_Data_Type = Header->Strings[4].SubString(Header->Strings[4].Pos("=")+1,Header->Strings[4].Pos(";")-Header->Strings[4].Pos("=")-1).Trim();
                if_header->Reference_Document =Header->Strings[5].SubString(Header->Strings[5].Pos("=")+1,Header->Strings[5].Pos(";")-Header->Strings[5].Pos("=")-1).Trim();
                if_header->Reference_Software =Header->Strings[6].SubString(Header->Strings[6].Pos("=")+1,Header->Strings[6].Pos(";")-Header->Strings[6].Pos("=")-1).Trim();
                if_header->Operating_System =Header->Strings[7].SubString(Header->Strings[7].Pos("=")+1,Header->Strings[7].Pos(";")-Header->Strings[7].Pos("=")-1).Trim();
                if_header->Product_Creation_Time =Header->Strings[8].SubString(Header->Strings[8].Pos("=")+1,Header->Strings[8].Pos(";")-Header->Strings[8].Pos("=")-1).Trim();

                if_header->Mission_Name = Header->Strings[10].SubString(Header->Strings[10].Pos("=")+1,Header->Strings[10].Pos(";")-Header->Strings[10].Pos("=")-1).Trim();
                if_header->Altimeter_Sensor_Name =Header->Strings[11].SubString(Header->Strings[11].Pos("=")+1,Header->Strings[11].Pos(";")-Header->Strings[11].Pos("=")-1).Trim();
                if_header->Radiometer_Sensor_Name =Header->Strings[12].SubString(Header->Strings[12].Pos("=")+1,Header->Strings[12].Pos(";")-Header->Strings[12].Pos("=")-1).Trim();
                if_header->DORIS_Sensor_Name =Header->Strings[13].SubString(Header->Strings[13].Pos("=")+1,Header->Strings[13].Pos(";")-Header->Strings[13].Pos("=")-1).Trim();
                if_header->Acquisition_Station_Name =Header->Strings[14].SubString(Header->Strings[14].Pos("=")+1,Header->Strings[14].Pos(";")-Header->Strings[14].Pos("=")-1).Trim();
                if_header->Cycle_Number =Header->Strings[15].SubString(Header->Strings[15].Pos("=")+1,Header->Strings[15].Pos(";")-Header->Strings[15].Pos("=")-1).Trim();
                if_header->Absolute_Revolution_Number =Header->Strings[16].SubString(Header->Strings[16].Pos("=")+1,Header->Strings[16].Pos(";")-Header->Strings[16].Pos("=")-1).Trim();
                if_header->Pass_Number =Header->Strings[17].SubString(Header->Strings[17].Pos("=")+1,Header->Strings[17].Pos(";")-Header->Strings[17].Pos("=")-1).Trim();
                if_header->Absolute_Pass_Number =Header->Strings[18].SubString(Header->Strings[18].Pos("=")+1,Header->Strings[18].Pos(";")-Header->Strings[18].Pos("=")-1).Trim();
                if_header->Equator_Time =Header->Strings[19].SubString(Header->Strings[19].Pos("=")+1,Header->Strings[19].Pos(";")-Header->Strings[19].Pos("=")-1).Trim();
                if_header->Equator_Longitude =Header->Strings[20].SubString(Header->Strings[20].Pos("=")+1,Header->Strings[20].Pos("<")-Header->Strings[20].Pos("=")-1).Trim();
                if_header->First_Measurement_Time =Header->Strings[21].SubString(Header->Strings[21].Pos("=")+1,Header->Strings[21].Pos(";")-Header->Strings[21].Pos("=")-1).Trim();
                if_header->Last_Measurement_Time =Header->Strings[22].SubString(Header->Strings[22].Pos("=")+1,Header->Strings[22].Pos(";")-Header->Strings[22].Pos("=")-1).Trim();
                if_header->First_Measurement_Latitude =Header->Strings[23].SubString(Header->Strings[23].Pos("=")+1,Header->Strings[23].Pos("<")-Header->Strings[23].Pos("=")-1).Trim();
                if_header->Last_Measurement_Latitude =Header->Strings[24].SubString(Header->Strings[24].Pos("=")+1,Header->Strings[24].Pos("<")-Header->Strings[24].Pos("=")-1).Trim();
                if_header->First_Measurement_Longitude =Header->Strings[25].SubString(Header->Strings[25].Pos("=")+1,Header->Strings[25].Pos("<")-Header->Strings[25].Pos("=")-1).Trim();
                if_header->Last_Measurement_Longitude =Header->Strings[26].SubString(Header->Strings[26].Pos("=")+1,Header->Strings[26].Pos("<")-Header->Strings[26].Pos("=")-1).Trim();
                if_header->Pass_Data_Count =Header->Strings[27].SubString(Header->Strings[27].Pos("=")+1,Header->Strings[27].Pos(";")-Header->Strings[27].Pos("=")-1).Trim();
                if_header->Ocean_Pass_Data_Count =Header->Strings[28].SubString(Header->Strings[28].Pos("=")+1,Header->Strings[28].Pos(";")-Header->Strings[28].Pos("=")-1).Trim();
                if_header->Ocean_PCD =Header->Strings[29].SubString(Header->Strings[29].Pos("=")+1,Header->Strings[29].Pos("<")-Header->Strings[29].Pos("=")-1).Trim();
                if_header->Time_Epoch =Header->Strings[30].SubString(Header->Strings[30].Pos("=")+1,Header->Strings[30].Pos(";")-Header->Strings[30].Pos("=")-1).Trim();
                if_header->TAI_UTC_Difference =Header->Strings[31].SubString(Header->Strings[31].Pos("=")+1,Header->Strings[31].Pos(";")-Header->Strings[31].Pos("=")-1).Trim();
                if_header->Time_Of_Leap_Second =Header->Strings[32].SubString(Header->Strings[32].Pos("=")+1,Header->Strings[32].Pos(";")-Header->Strings[32].Pos("=")-1).Trim();
                if_header->Time_Shift_Mid_Frame =Header->Strings[33].SubString(Header->Strings[33].Pos("=")+1,Header->Strings[33].Pos("<")-Header->Strings[33].Pos("=")-1).Trim();
                if_header->Time_Shift_Interval =Header->Strings[34].SubString(Header->Strings[34].Pos("=")+1,Header->Strings[34].Pos("<")-Header->Strings[34].Pos("=")-1).Trim();
                if_header->Range_Offset =Header->Strings[35].SubString(Header->Strings[35].Pos("=")+1,Header->Strings[35].Pos("<")-Header->Strings[35].Pos("=")-1).Trim();
                if_header->Average_Pressure =Header->Strings[36].SubString(Header->Strings[36].Pos("=")+1,Header->Strings[36].Pos("<")-Header->Strings[36].Pos("=")-1).Trim();
                if_header->Header_Padding = Header->Strings[37].SubString(Header->Strings[37].Pos("=")+1,Header->Strings[37].Pos(";")-Header->Strings[37].Pos("=")-1).Trim();                                                                                                                                                                                          ;

                if_header->Altimeter_Level1 =Header->Strings[39].SubString(Header->Strings[39].Pos("=")+1,Header->Strings[39].Pos(";")-Header->Strings[39].Pos("=")-1).Trim();
                if_header->Radiometer_Level1 =Header->Strings[40].SubString(Header->Strings[40].Pos("=")+1,Header->Strings[40].Pos(";")-Header->Strings[40].Pos("=")-1).Trim();

                if_header->POSEIDON2_Characterization =Header->Strings[42].SubString(Header->Strings[42].Pos("=")+1,Header->Strings[42].Pos(";")-Header->Strings[42].Pos("=")-1).Trim();
                if_header->POSEIDON2_LTM =Header->Strings[43].SubString(Header->Strings[43].Pos("=")+1,Header->Strings[43].Pos(";")-Header->Strings[43].Pos("=")-1).Trim();
                if_header->JMR_Main_Beam =Header->Strings[44].SubString(Header->Strings[44].Pos("=")+1,Header->Strings[44].Pos(";")-Header->Strings[44].Pos("=")-1).Trim();
                if_header->JMR_BT_Averaging =Header->Strings[45].SubString(Header->Strings[45].Pos("=")+1,Header->Strings[45].Pos(";")-Header->Strings[45].Pos("=")-1).Trim();
                if_header->DORIS_TEC_Map =Header->Strings[46].SubString(Header->Strings[46].Pos("=")+1,Header->Strings[46].Pos(";")-Header->Strings[46].Pos("=")-1).Trim();
                if_header->DORIS_USO =Header->Strings[47].SubString(Header->Strings[47].Pos("=")+1,Header->Strings[47].Pos(";")-Header->Strings[47].Pos("=")-1).Trim();
                if_header->Orbit_Data =Header->Strings[48].SubString(Header->Strings[48].Pos("=")+1,Header->Strings[48].Pos(";")-Header->Strings[48].Pos("=")-1).Trim();
                if_header->PF_Corrections =Header->Strings[49].SubString(Header->Strings[49].Pos("=")+1,Header->Strings[49].Pos(";")-Header->Strings[49].Pos("=")-1).Trim();
                if_header->Pole_Location =Header->Strings[50].SubString(Header->Strings[50].Pos("=")+1,Header->Strings[50].Pos(";")-Header->Strings[50].Pos("=")-1).Trim();
                if_header->MTO_Fields =Header->Strings[51].SubString(Header->Strings[51].Pos("=")+1,Header->Strings[51].Pos(";")-Header->Strings[51].Pos("=")-1).Trim();
                if_header->ORF_Data = Header->Strings[52].SubString(Header->Strings[52].Pos("=")+1,Header->Strings[52].Pos(";")-Header->Strings[52].Pos("=")-1).Trim();
                if_header->POSEIDON2_OB_RET_Correction_Tables =Header->Strings[53].SubString(Header->Strings[53].Pos("=")+1,Header->Strings[53].Pos(";")-Header->Strings[53].Pos("=")-1).Trim();
                if_header->POSEIDON2_SSB =Header->Strings[54].SubString(Header->Strings[54].Pos("=")+1,Header->Strings[54].Pos(";")-Header->Strings[54].Pos("=")-1).Trim();
                if_header->POSEIDON2_Composite_SSB =Header->Strings[55].SubString(Header->Strings[55].Pos("=")+1,Header->Strings[55].Pos(";")-Header->Strings[55].Pos("=")-1).Trim();
                if_header->JMR_Retrieval_Coefficients =Header->Strings[56].SubString(Header->Strings[56].Pos("=")+1,Header->Strings[56].Pos(";")-Header->Strings[56].Pos("=")-1).Trim();
                if_header->LAND_SEA_Mask_Map =Header->Strings[57].SubString(Header->Strings[57].Pos("=")+1,Header->Strings[57].Pos(";")-Header->Strings[57].Pos("=")-1).Trim();
                if_header->Ocean_Tide_Sol_1 =Header->Strings[58].SubString(Header->Strings[58].Pos("=")+1,Header->Strings[58].Pos(";")-Header->Strings[58].Pos("=")-1).Trim();
                if_header->Ocean_Tide_Sol_2 =Header->Strings[59].SubString(Header->Strings[59].Pos("=")+1,Header->Strings[59].Pos(";")-Header->Strings[59].Pos("=")-1).Trim();
                if_header->Tidal_Loading_Sol_1 =Header->Strings[60].SubString(Header->Strings[60].Pos("=")+1,Header->Strings[60].Pos(";")-Header->Strings[60].Pos("=")-1).Trim();
                if_header->Tidal_Loading_Sol_2 =Header->Strings[61].SubString(Header->Strings[61].Pos("=")+1,Header->Strings[61].Pos(";")-Header->Strings[61].Pos("=")-1).Trim();
                if_header->Solid_Earth_Tide =Header->Strings[62].SubString(Header->Strings[62].Pos("=")+1,Header->Strings[62].Pos(";")-Header->Strings[62].Pos("=")-1).Trim();
                if_header->NEQ_Tide =Header->Strings[63].SubString(Header->Strings[63].Pos("=")+1,Header->Strings[63].Pos(";")-Header->Strings[63].Pos("=")-1).Trim();
                if_header->Geoid_Map =Header->Strings[64].SubString(Header->Strings[64].Pos("=")+1,Header->Strings[64].Pos(";")-Header->Strings[64].Pos("=")-1).Trim();
                if_header->MSS_Map =Header->Strings[65].SubString(Header->Strings[65].Pos("=")+1,Header->Strings[65].Pos(";")-Header->Strings[65].Pos("=")-1).Trim();
                if_header->Bathymetry_Topography_Map =Header->Strings[66].SubString(Header->Strings[66].Pos("=")+1,Header->Strings[66].Pos(";")-Header->Strings[66].Pos("=")-1).Trim();

/*                info.MinLon=StrToFloat(if_header->First_Measurement_Longitude);
                info.MaxLon=StrToFloat(if_header->Last_Measurement_Longitude);
                info.MinLat=StrToFloat(if_header->First_Measurement_Latitude);
                info.MaxLat=StrToFloat(if_header->Last_Measurement_Latitude);

                info.NumRec=StrToInt(if_header->Pass_Data_Count);

                Buffer=if_header->First_Measurement_Time;

                yy=StrToInt(Buffer.SubString(1,Buffer.Pos("-")-1));
                Buffer=Buffer.SubString(Buffer.Pos("-")+1,Buffer.Length());
                mo=StrToInt(Buffer.SubString(1,Buffer.Pos("-")-1));
                Buffer=Buffer.SubString(Buffer.Pos("-")+1,Buffer.Length());
                dd=StrToInt(Buffer.SubString(1,Buffer.Pos("T")-1));
                Buffer=Buffer.SubString(Buffer.Pos("T")+1,Buffer.Length());

                hh=StrToInt(Buffer.SubString(1,Buffer.Pos(":")-1));
                Buffer=Buffer.SubString(Buffer.Pos(":")+1,Buffer.Length());
                mm=StrToInt(Buffer.SubString(1,Buffer.Pos(":")-1));
                Buffer=Buffer.SubString(Buffer.Pos(":")+1,Buffer.Length());
                ss=StrToInt(Buffer.SubString(1,Buffer.Pos(".")-1));
                Buffer=Buffer.SubString(Buffer.Pos(".")+1,Buffer.Length());
//                ms=StrToInt(Buffer.SubString(1,Buffer.Length()));
                ms=0;

                info.fd=TDateTime(yy,mo,dd)+TDateTime(hh,mm,ss,ms);

                Buffer=if_header->Last_Measurement_Time;

                yy=StrToInt(Buffer.SubString(1,Buffer.Pos("-")-1));
                Buffer=Buffer.SubString(Buffer.Pos("-")+1,Buffer.Length());
                mo=StrToInt(Buffer.SubString(1,Buffer.Pos("-")-1));
                Buffer=Buffer.SubString(Buffer.Pos("-")+1,Buffer.Length());
                dd=StrToInt(Buffer.SubString(1,Buffer.Pos("T")-1));
                Buffer=Buffer.SubString(Buffer.Pos("T")+1,Buffer.Length());

                hh=StrToInt(Buffer.SubString(1,Buffer.Pos(":")-1));
                Buffer=Buffer.SubString(Buffer.Pos(":")+1,Buffer.Length());
                mm=StrToInt(Buffer.SubString(1,Buffer.Pos(":")-1));
                Buffer=Buffer.SubString(Buffer.Pos(":")+1,Buffer.Length());
                ss=StrToInt(Buffer.SubString(1,Buffer.Pos(".")-1));
                Buffer=Buffer.SubString(Buffer.Pos(".")+1,Buffer.Length());
//                ms=StrToInt(Buffer.SubString(1,Buffer.Length()));
                ms=0;

                info.ld=TDateTime(yy,mo,dd)+TDateTime(hh,mm,ss,ms);  */
               delete Header;
        }
        catch (...)
        {
                return false;
        }

return true;
}

//---------------------------------------------------------------------------
BOOL JasonThread::GETDATETIMEDVUCUR(AnsiString String)
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


//----------------------------------------------------------------------------
void JasonThread::GETBITS(char zn,bool *bits)
{
char tempzn=0;
int i=0;

        tempzn=zn;

        for (i=7;i>=0;i--)
        {
                if (tempzn>0)
                {
                        if (tempzn-pow(2,(int)i)>=0)
                        {
                                tempzn-=pow(2,(int)i);
                                bits[i]=1;
                        }
                        else
                                bits[i]=0;
                }
                else
                        break;
        }

}
//----------------------------------------------------------------------------
AnsiString JasonThread::DATETIMEASTIMATION(int days,int s,int mcs)
{
TDateTime rdt;
double topex_dt;
AnsiString Out;


        topex_dt=days+s/86400.+mcs/86400000000.;

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
