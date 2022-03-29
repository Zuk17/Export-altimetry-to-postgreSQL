//---------------------------------------------------------------------------

#include <vcl.h>
#pragma hdrstop

#include "ThreadEnvisat.h"

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
__fastcall EnviThread::EnviThread(bool CreateSuspended)
        : TThread(CreateSuspended)
{
        FreeOnTerminate=true;
}
//---------------------------------------------------------------------------
void __fastcall EnviThread::Execute()
{
Count_Errors=0;

        List_Debug=new TStringList;
        List_Debug->Clear();

        DirList=new TStringList;
        ListOut=new TStringList;
        ListOut->Clear();
        dt1958=TDateTime(2000,01,01);
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
        return;

}
//---------------------------------------------------------------------------
void EnviThread::CalculatingGDR (AnsiString Path_to_DATA)
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
                FILE *in;
                unsigned char *buf;
                long  start_of_data_offset;
				buf=new unsigned char[J1GDR_RECSIZE];
				long offset_data=0,length=0,length1=0;

               int   nrec,ctrl_read,ctrl_read1;

        //Считывание и обработка файла
                for (int i=0;i<DirList->Count;i++,Alt_start->Progress_mgdr_dir->Position++)
				{
 //                 filelen=0;

                        if ((in = fopen((DirList->Strings[i].c_str()), "r")) < 0)
								return;

						  offset_data=0;

						  fseek(in, 0L, SEEK_END);
						  length=ftell(in);
						  fseek(in, 0L,SEEK_SET);
while (true){

						  length1=ftell(in);

						  ctrl_read=fread(&entete,sizeof(entete),1,in);

						  length1=ftell(in);

						  if (ctrl_read != 0)
						  {
						   if (entete.nbr_point<1000) {
							   Alt_start->Progress_mgdr_file->Min=0;
							   Alt_start->Progress_mgdr_file->Max=entete.nbr_point-1;

							   for (nrec = 0; nrec < entete.nbr_point; nrec++,Alt_start->Progress_mgdr_file->Position++)
								{
									length1=ftell(in);

									ctrl_read1=fread(&envigdr,sizeof(envigdr),1,in);

									length1=ftell(in);

									AltCount();
								}
							   Alt_start->Progress_mgdr_file->Position=0;
							   Alt_start->Refresh();
							   if (length1>length) break;
							}
							else 	MessageDlg("bitie dannie",mtError,TMsgDlgButtons() << mbOK,0);
						   }
}
						fclose(in);
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
BOOL EnviThread::CreatingTleDB (AnsiString Path_to_Tle)
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
  //Создание массива данных TLE для Envi1
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
  //Поиск строк для Envi1
                 while (fgets(msg, 256, iin)!=NULL)
                       if (strncmp(msg,"1 27386U",8)==0) break;
  //Занесение строк в массив
                 if (strncmp("1 27386U",msg,8)==0)
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
BOOL EnviThread::AltCount(void)
{
AnsiString Buffer;
AnsiString BC;
int k=0;
long h_orb[20],h_range[20];
int j=0;
ofstream fout;


/*-----------------------------------------------*/


     latitude=(double)envigdr.latitude/1000000.;
     if (envigdr.longitude/1000000.>=180.0)
          longitude=(double)((double)envigdr.longitude/1000000-360);
      else
          longitude=(double)envigdr.longitude/1000000.;

     if (((latitude>45) && (latitude<54) && (longitude>117.6) && (longitude<140)) ||
     	 	((latitude>65) && (latitude<68) && (longitude>65) && (longitude<68)))
//     if (j1gdr.surface_type==0) // open oceans or semi-enclosed seas
     {


//        int H_REF=StrToInt(if_header->Range_Offset)*1000*10000; //Пиздец!я хуею дороая редакция!!!
// Orbit
//        if (envigdr.alt_cog_ellip!=4294967295 &&(envigdr.alt_cog_ellip>=300000000&&envigdr.alt_cog_ellip<=700000000)) // altitude is good
        {
		H_SAT=long(envigdr.alt_cog_ellip);

                for (k=0;k<20;k++)
                        h_orb[k]=H_SAT+long(envigdr.hz18_diff_1hz_alt[k]);
        }
//        else {H_SAT=4294967295;Count_Errors++;return true;}

//Altimeter range
//        if (envigdr.ku_band_ocean_range!=4294967295 &&(envigdr.ku_band_ocean_range>=300000000&&envigdr.ku_band_ocean_range<=700000000)) // altitude is good
        {
                H_Alt=long(envigdr.ku_band_ocean_range);

                for (k=0;k<20;k++)
                        h_range[k]=H_Alt+long(envigdr.hz18_ku_band_ocean_range[k]);
        }
//        else {H_Alt=4294967295;Count_Errors++;return true;}

//Ionosphere correction
        if (envigdr.ra2_ion_corr_ku!=32767 ) // iono_corr is good
                Iono_Cor=envigdr.ra2_ion_corr_ku;
        else Iono_Cor=32767;

        if (envigdr.ion_corr_doris_ku<=0&&envigdr.ion_corr_doris_ku>=-1000)
                Iono_Dor=envigdr.ion_corr_doris_ku;
        else Iono_Dor=32767;

// Dry correction
        if (envigdr.mod_dry_tropo_corr!=32767 &&(envigdr.mod_dry_tropo_corr>=-25000&&envigdr.mod_dry_tropo_corr<=-19000)) // dry_cor is good
		Dry_Cor=envigdr.mod_dry_tropo_corr;
        else Dry_Cor=32767;
// Wet correction
        if (envigdr.mod_wet_tropo_corr!=32767 &&(envigdr.mod_wet_tropo_corr>=-5000&&envigdr.mod_wet_tropo_corr< -10)) // wet_corr is good
                Wet_Cor=envigdr.mod_wet_tropo_corr;
        else Wet_Cor=32767;

        Wet1_Cor=32767;
        Wet2_Cor=32767;

        if (envigdr.mwr_wet_tropo_corr!=32767 &&(envigdr.mwr_wet_tropo_corr>=-5000&&envigdr.mwr_wet_tropo_corr< -10)) // wet_corr is good
                Wet_H=envigdr.mwr_wet_tropo_corr;
        else Wet_H=32767;


// Solid Earth Tide
        if (envigdr.solid_earth_tide_ht!=32767 &&(envigdr.solid_earth_tide_ht>=-10000&&envigdr.solid_earth_tide_ht<=10000)) // solid earth tide is good
                H_SET=envigdr.solid_earth_tide_ht;
        else H_SET=32767;

// Geocentric Pole Tide
/*        if (envigdr.pole_tide!=32767 &&(envigdr.pole_tide>=-1000&&envigdr.pole_tide<=1000)) // pole_tide is good
                H_POL=envigdr.pole_tide;
        else H_POL=32767;*/
        H_POL=0;

//Geoid
        if (envigdr.geoid_ht!=2147483647 &&(envigdr.geoid_ht>=-1500000&&envigdr.geoid_ht<=1500000)) // geoid is good
                H_GEO=envigdr.geoid_ht;
        else H_GEO=2147483647;
// Inverse Barometer Effect
        if (envigdr.inv_barom_corr!=32767) // inv_bar is good
                INV_BAR=envigdr.inv_barom_corr;
        else INV_BAR=32767;


//-------------------------------->

//        Buffer.sprintf("%s,%.3f,%.3f,%.2f,%s,%s",DATETIMEASTIMATION(envigdr.time_day,envigdr.time_sec,envigdr.time_microsec),longitude,latitude,(H_SAT-H_Alt-Wet_Cor-Dry_Cor-Iono_Cor-ssb_ku-mss-INV_BAR-high_freq_wind_resp-ocean_tide-H_SET-H_POL)/100.,if_header->Cycle_Number,if_header->Pass_Number); //перевод в метры
        Buffer.sprintf("insert into alt_data_1hz values ('EN',%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,cast('%s' as timestamp),GeomFromText('POINT(%.3f %.3f)',32652),%d,%d)",longitude,latitude,(H_SAT-H_Alt)/100.,Iono_Cor/100.,Iono_Dor/100.,Dry_Cor/100.,Wet_Cor/100.,Wet1_Cor/100.,Wet2_Cor/100.,Wet_H/100.,H_GEO/100.,H_SET/1000.,H_POL/100.,INV_BAR/100.,DATETIMEASTIMATION(envigdr.dsr_time_day,envigdr.dsr_time_sec,envigdr.dsr_time_microsec),longitude,latitude,entete.cycle,entete.trace); //перевод в метры
//        List_Debug->Add(Buffer);

        Alt_start->res = Alt_start->PQexec(Alt_start->conn, Buffer.c_str());
        if (Alt_start->PQresultStatus(Alt_start->res) != PGRES_COMMAND_OK)
             MessageDlg(Alt_start->PQerrorMessage(Alt_start->conn),mtError,TMsgDlgButtons() << mbOK,0);
        Alt_start->PQclear(Alt_start->res);


//Calculating Time
int year=2000;
int day=1;
int delta=envigdr.dsr_time_day;
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
        if ((TleData_List[j].day<delta) || ((TleData_List[j].day==delta)&&(TleData_List[j].time<(envigdr.dsr_time_sec+envigdr.dsr_time_microsec/1000000))))
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


           time_tmp=double(envigdr.dsr_time_microsec)/1000000.-17/36.;
           time_tmp+=double(envigdr.dsr_time_sec);

            for (k=0;k<20;k++)
           {
//              if((h_orb[k]-h_range[k]<100000)&&(h_orb[k]-h_range[k]>-100000)){
                time_tmp+=1/18.;
                //время в минутах с эпохи!!! функция возвращает координату в eci
                orbit.getPosition((double(time_tmp-Tle_work.time)/60+double(delta-Tle_work.day)*1440), &eci);
                //преобразует координату
                FreshCoord=eci.toGeo();
//                alt_land[i]=h_orb[i]-(h_range[i]+Dry_Cor+Iono_Cor)-H_GEO-H_SET;

                Buffer.sprintf("insert into alt_data_10hz values ('EN',%.3f,%.3f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,cast('%s' as timestamp),GeomFromText('POINT(%.3f %.3f)',32652),%d,%d)",FreshCoord.m_Lon*180/PI,FreshCoord.m_Lat*180/PI,(h_orb[k]-h_range[k])/100.,Iono_Cor/100.,Iono_Dor/100.,Dry_Cor/100.,Wet_Cor/100.,Wet1_Cor/100.,Wet2_Cor/100.,Wet_H/100.,H_GEO/100.,H_SET/100.,H_POL/100.,INV_BAR/100.,DATETIMEASTIMATION(envigdr.dsr_time_day,int(time_tmp),0),FreshCoord.m_Lon*180/PI,FreshCoord.m_Lat*180/PI,entete.cycle,entete.trace); //перевод в сантиметры
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

//---------------------------------------------------------------------------
BOOL EnviThread::GETDATETIMEDVUCUR(AnsiString String)
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
void EnviThread::GETBITS(char zn,bool *bits)
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
AnsiString EnviThread::DATETIMEASTIMATION(int days,int s,int mcs)
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
