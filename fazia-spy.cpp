#include <iostream>
#include <iomanip>

#include "boost/program_options.hpp"
#include "zmq.hpp"

#include "FzEventSet.pb.h"

#include <TApplication.h>
#include <TCanvas.h>
#include <TAxis.h>
#include <TF1.h>
#include <TGraphPolar.h>
#include <TGraph.h>
#include <TGClient.h>
#include <TMath.h>
#include <TROOT.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TStyle.h>
#include <TThread.h>


#define TEL_HIST_NUM	 6
#define TOT_HIST_NUM	96

namespace po = boost::program_options;

// GLOBAL vars

TCanvas *theCanvas;
TH1F *adcHistogram;
TH1F *phHistogram;
TH1F *telHistograms[TEL_HIST_NUM];
TH1F *totHistograms[TOT_HIST_NUM];

TThread *displayupdatethread=NULL;
TThread *histogramfillthread=NULL;

static char const* const FzFec_str[] = { "FEC#0", "FEC#1", "FEC#2", "FEC#3", "FEC#4", "FEC#5", "FEC#6", "FEC#7", "", "", "", "", "", "", "", "ADC"  };
static char const* const FzDataType_str[] = { "QH1", "I1", "QL1", "Q2", "I2", "Q3", "ADC", "UNK" };
static char const* const FzTelescope_str[] = { "A", "B" };
static char const* const FzDetector_str[] = { "Si1", "Si2", "CsI" };

bool total;	// total view of BLOCK
bool adc;	// acquire ADC from BLOCK
unsigned int nblk, nfee, ntel;

void dumpOnScreen(DAQ::FzEvent *ev) {

   DAQ::FzBlock *blk;
   DAQ::FzFee *fee;
   DAQ::FzHit *hit;
   DAQ::FzData *d;
   DAQ::Energy *en;
   DAQ::Waveform *wf;

   std::cout << "EC: " << ev->ec() << std::endl;

   for (int i = 0; i < ev->block_size(); i++) {

      const DAQ::FzBlock& rdblk = ev->block(i);

      if(rdblk.has_len_error() && rdblk.has_crc_error() )
         std::cout << "\tBLKID: " << rdblk.blkid() << " - LEN_ERR: " << rdblk.len_error() << " - CRC_ERR: " << rdblk.crc_error() << std::endl;
      else
         std::cout << "\tBLKID: " << rdblk.blkid() << std::endl;

      for(int j = 0; j < rdblk.fee_size(); j++) {

         const DAQ::FzFee& rdfee = rdblk.fee(j);
         std::cout << "\t\t" << FzFec_str[rdfee.feeid()] << " - LEN_ERR: " << rdfee.len_error() << " - CRC_ERR: " << rdfee.crc_error() << std::endl;

         for(int k = 0; k < rdfee.hit_size(); k++) {

            const DAQ::FzHit& rdhit = rdfee.hit(k);
            std::cout << "\t\t\tEC: " << rdhit.ec();

            std::cout << " - TEL: ";
            if(rdhit.has_telid())
               std::cout << FzTelescope_str[rdhit.telid()];
            else { std::cout << "<ERR>" << std::endl; continue; }

            std::cout << " - DET: ";
            if(rdhit.has_detid())
               std::cout << FzDetector_str[rdhit.detid()];
            else { std::cout << "<ERR>" << std::endl; continue; }

            std::cout << " - FEE: ";
            if(rdhit.has_feeid())
               std::cout << FzFec_str[rdhit.feeid()];
            else { std::cout << "<ERR>" << std::endl; continue; }

            std::cout << " - GTTAG: " << rdhit.gttag();
            std::cout << " - DETTAG: " << rdhit.dettag() << std::endl;

            for(int m = 0; m < rdhit.data_size(); m++) {

               const DAQ::FzData& rdata = rdhit.data(m);
               std::cout << "\t\t\t\tDATATYPE: " << FzDataType_str[rdata.type()];

               if(!rdata.has_energy() && !rdata.has_waveform()) {
                  std::cout << " [NO DATA]" << std::endl;
                  continue;
               }

               if(rdata.has_energy()) {

                  const DAQ::Energy& ren = rdata.energy();
                  std::cout << " - ENERGY: ";

                  for(int e=0; e < ren.value_size(); e++)
                     std::cout << ren.value(e) << ", ";

                  std::cout << " - LEN_ERR: " << ren.len_error();
               }

               if(rdata.has_waveform()) {

                 const DAQ::Waveform& rwf = rdata.waveform();
                 std::cout << " - PRETRIG: " << rwf.pretrig();

                 std::cout << " - WAVEFORM: " << " - LEN_ERR: " << rwf.len_error() << std::endl;
                 std::cout << std::endl << "\t\t\t\t              ";
                 signed int supp;
                 for(int n=0; n < rwf.sample_size(); n++) {
                    if(n%16 == 0 && n)
                       std::cout << std::endl << "\t\t\t\t              ";

                    if(rdata.type() != DAQ::FzData::ADC) {

                       if(rwf.sample(n) > 8191)
                          supp = rwf.sample(n) | 0xFFFFC000;
                       else
                          supp = rwf.sample(n);

                    } else supp = rwf.sample(n);

                    std::cout << supp << ", ";
                 }
               }

               std::cout << std::endl;

            }
         }
      }
   }

   std::cout << "### END of event ###" << std::endl;
}

// ### DISPLAY UPDATE THREAD ###

void *displayupdate(void *var) { 

   while (1) { 

      usleep(100000); // delay 1/10 second

      if(total) {

         for(int i=0; i<TOT_HIST_NUM; i++) {

            theCanvas->cd(i + 1);
            gPad->Modified();
            //gPad->Update();
         }

      } else {

         if(adc) {

            theCanvas->cd(1);
            gPad->Modified();

            theCanvas->cd(2);
            gPad->Modified();

         } else {

            for(int i=0; i<TEL_HIST_NUM; i++) { 

               theCanvas->cd(i + 1);
               gPad->Modified();
               //gPad->Update();
            }
         }
      }

      theCanvas->cd(0);
      theCanvas->Update();
    }
}

int connectdisplayupdate(void *var) { 

  displayupdatethread = new TThread("displayupdatethread",displayupdate,var);

  if  (displayupdatethread==NULL) { 

      printf("the displayupdate thread did not start\n");
      return(-1);

  } else {
 
    displayupdatethread->Run();
    return(0);
  }
}

// ### HISTOGRAM FILL THREAD ###

void *histogramfill(void *var) {

  zmq::socket_t *zmq_sub = (zmq::socket_t *) var;
  zmq::message_t event;

  DAQ::FzEvent ev;

  while (1) {

     zmq_sub->recv(&event);

     if(ev.ParseFromArray(event.data(), event.size()) == true) {

         std::cout << "parsing succedeed ! - event size = " << event.size() << std::endl;

         for (int i=0; i<ev.block_size(); i++) {         

            const DAQ::FzBlock& rdblk = ev.block(i);

            if(rdblk.blkid() != nblk)
	       continue;

            for(int j=0; j<rdblk.fee_size(); j++) {

               const DAQ::FzFee& rdfee = rdblk.fee(j);

               if((!adc) && (!total) && (rdfee.feeid() != nfee))
                  continue;

               for(int k=0; k<rdfee.hit_size(); k++) {

                  const DAQ::FzHit& rdhit = rdfee.hit(k);

                  if((!adc) && (!total) && (rdhit.telid() != ntel))
                     continue;

                  for(int m=0; m<rdhit.data_size(); m++) {

                     const DAQ::FzData& rdata = rdhit.data(m);
                     const DAQ::Waveform& rwf = rdata.waveform();

		     long long int ssin, scos;
                     unsigned long long int usin = (rwf.sample(0) * pow(2,22)) + (rwf.sample(1) * pow(2,7)) + (rwf.sample(2) / pow(2,8));
                     unsigned long long int ucos = (rwf.sample(3) * pow(2,22)) + (rwf.sample(4) * pow(2,7)) + (rwf.sample(5) / pow(2,8));

                     if(rwf.sample(0) & 0x4000)
                        ssin = usin + 0xFFFFFFE000000000;
		     else 
                        ssin = usin;

                     if(rwf.sample(3) & 0x4000)
                        scos = ucos + 0xFFFFFFE000000000;
		     else
 			scos = ucos;

                     double refmod = sqrt( pow(ssin,2) + pow(scos,2) ) / pow(10,6);
                     double phase = atan2(ssin, scos);

                     phase = (360 * phase / (2*3.14)) + 180;

                     std::cout << "usin(0) " << rwf.sample(0) << std::endl; 
                     std::cout << "usin(1) " << rwf.sample(1) << std::endl; 
                     std::cout << "usin(2) " << rwf.sample(2) << std::endl; 

                     std::cout << "ucos(0) " << rwf.sample(3) << std::endl; 
                     std::cout << "ucos(1) " << rwf.sample(4) << std::endl; 
                     std::cout << "ucos(2) " << rwf.sample(5) << std::endl; 
                      
                     std::cout << std::dec;
                     std::cout << "sin = " << ssin << " - cos = " << scos << std::endl;

                     printf("refmod = %f - phase = %f\n", refmod, phase);

                     phHistogram->Fill(phase); 
 
                     signed int supp;
                     
                     for(int n=0; n<rwf.sample_size(); n++) {

                        if(rdata.type() != DAQ::FzData::ADC) {

                           if(rwf.sample(n) > 8191)
                              supp = rwf.sample(n) | 0xFFFFC000;
                           else
                              supp = rwf.sample(n);

                        } else { 
		       
                           supp = rwf.sample(n);
                        }

                        //if(adc && (n>5)) {
                        if(adc) {

                           adcHistogram->SetBinContent(n, (Double_t)supp);
                          
                        } else {

                           if(total) {
 
                              totHistograms[(rdfee.feeid()*12)+(rdhit.telid()*6)+rdata.type()]->SetBinContent(n, (Double_t)supp);
                              //totHistograms[(rdfee.feeid()*12)+(rdhit.telid()*6)+rdata.type()]->Fill((Double_t)n, (Double_t)supp);
 
                           } else {
                       
                              telHistograms[rdata.type()]->SetBinContent(n, (Double_t)supp);
                              //telHistograms[rdata.type()]->Fill((Double_t)n, (Double_t)supp);
                           }
                        }

                     } // end of SAMPLE set

                  } // end of DATA set

               } // end of HIT/CHANNEL set

            } // end of FEE set

         } // end of BLOCK set
     }
  }

  pthread_exit(NULL);// (code execution never gets here)
}

int connecthistogramfill(void *var) { 

  histogramfillthread = new TThread("histogramupdatethread",histogramfill, (zmq::socket_t *) var);

  if (histogramfillthread==NULL) { 

     printf("the histogramfill thread did not start\n");
     return(-1);

  } else {

     histogramfillthread->Run();
     return(0);
  }
}

int main(int argc, char *argv[]) {

   zmq::context_t zmq_ctx(1);
   zmq::socket_t zmq_sub(zmq_ctx, ZMQ_SUB);
   zmq::message_t event;

   DAQ::FzEvent ev;
   std::string hostname, port;
   std::string url;

   // handling of command line parameters
   po::options_description desc("\nFazia Spy - allowed options");

   desc.add_options()
    ("help", "produce help message")
    ("host", po::value<std::string>(), "FzDAQ publisher hostname to contact")
    ("port", po::value<std::string>(), "[optional] FzDAQ publisher port (default: 5563)")
    ("single", "[optional] single shot, dump first acquired event to screen")
    ("adc", "[optional] acquire only ADC from BLOCK")
    ("block", po::value<unsigned int>(), "BLOCK card number to spy (default: 0) (if specify only BLOCK a total view will be plotted)")
    ("fee", po::value<unsigned int>(), "FRONTEND card number to spy [0...7] (default: 0)")
    ("tel", po::value<unsigned int>(), "TELESCOPE to spy [0 => A , 1 => B] (default: 0)")
   ;

   po::variables_map vm;
   po::store(po::parse_command_line(argc, argv, desc), vm);
   po::notify(vm);

   if (vm.count("help")) {
      std::cout << desc << "\n";
      return 0;
   }

   if (vm.count("host")) {
      hostname = vm["host"].as<std::string>();
   } else {
      std::cout << "ERROR: host missing" << std::endl;
      return -1;
   }

   if (vm.count("port")) {
      port = vm["port"].as<std::string>(); 
   } else {
      port = "5563";
   }

   url = "tcp://" + hostname + ":" + port;

   std::cout << "connect to: " << url << std::endl;

   try { 

      zmq_sub.connect(url.c_str()); 

   } catch(zmq::error_t zmq_err) {

      std::cout << "ERROR: connection failed - " << zmq_err.what() << std::endl;
      return -1;
   }

   zmq_sub.setsockopt(ZMQ_SUBSCRIBE, "", 0);

   if (vm.count("single")) {

      zmq_sub.recv(&event);

      if(ev.ParseFromArray(event.data(), event.size()) == true) {

         std::cout << "parsing succedeed ! - event size = " << event.size() << std::endl;
         dumpOnScreen(&ev);
      }

      return 0;
   }


   if(vm.count("block")) {
      nblk = vm["block"].as<unsigned int>();
   } else {
      nblk = 0;
   }

   total = true;

   if(vm.count("adc")) {
      adc = true;
      total = false;
   }

   if(vm.count("fee")) {
      nfee = vm["fee"].as<unsigned int>();

      if( (nfee > 7) || (nfee < 0) ) {

         std::cout << "ERROR: FRONTEND card number out of range" << std::endl;
         return -1;       
      }

      total = false;   
   } 

   if(vm.count("tel")) {
      ntel = vm["tel"].as<unsigned int>();

      if( (ntel > 1) || (ntel < 0) ) {

         std::cout << "ERROR: TELESCOPE number out of range" << std::endl;
         return -1;       
      }
         
      total = false;
   } 

   if(adc)
      std::cout << "starting FAZIA SPY GUI interface for: " << "B" << nblk << " ADC channel" << std::endl;
   else 
      std::cout << "starting FAZIA SPY GUI interface for: " << "B" << nblk << "-" << "F" << nfee << "-" << "T" << ntel << std::endl;

   TApplication* rootapp = new TApplication("FAZIA SPY GUI", &argc, argv);

   //theCanvas is allocated for the life of the program:
   Double_t w = 1024;
   Double_t h = 768;
   theCanvas = new TCanvas("theCanvas", "the Canvas", w, h);
   theCanvas->SetWindowSize(w + (w - theCanvas->GetWw()), h + (h - theCanvas->GetWh()));
  
   if(total) {

      for(int i=0; i<8; i++) {	// for each Frontend Card
         for(int j=0; j<2; j++) { // for each Telescope

            totHistograms[(i*12)+DAQ::FzData::QH1+(j*6)] = new TH1F("QH1", "QH1", 1024, 0, 1024);
            totHistograms[(i*12)+DAQ::FzData::I1+(j*6)] = new TH1F("I1", "I1", 500, 0, 500);
            totHistograms[(i*12)+DAQ::FzData::QL1+(j*6)] = new TH1F("QL1", "QL1", 1024, 0, 1024);
            totHistograms[(i*12)+DAQ::FzData::Q2+(j*6)] = new TH1F("Q2", "Q2", 1024, 0, 1024);
            totHistograms[(i*12)+DAQ::FzData::I2+(j*6)] = new TH1F("I2", "I2", 500, 0, 500);
            totHistograms[(i*12)+DAQ::FzData::Q3+(j*6)] = new TH1F("Q3", "Q3", 1024, 0, 1024);
         }
      }

      // create the sub pads:
      theCanvas->Divide(12, 8);

      // draw the histograms once
      for(int i=0; i<8; i++) {  // for each Frontend Card
         for(int j=0; j<2; j++) { // for each Telescope
            for(int k=0; k<6; k++) {

               std::cout << "index = " << (i*12)+k+(j*6) << std::endl;
               theCanvas->cd((i*12)+k+(j*6)+1);
	       totHistograms[(i*12)+k+(j*6)]->SetMaximum(8500);
	       totHistograms[(i*12)+k+(j*6)]->SetMinimum(-8500);
               totHistograms[(i*12)+k+(j*6)]->Draw();
            }
         }
      }

   } else {

      if(adc) {

         adcHistogram = new TH1F("ADC", "ADC", 150, 0, 150);   
         adcHistogram->SetMaximum(8000);
         adcHistogram->SetMinimum(8000);

         phHistogram = new TH1F("Phase", "Phase", 100000, 0, 360);
                   
         // create the sub pads:
         theCanvas->Divide(2, 1);

         theCanvas->cd(1);
         adcHistogram->Draw();

         theCanvas->cd(2);
         phHistogram->Draw();

      } else {

         // telHistograms[i] are allocated for the life of the program:
         telHistograms[DAQ::FzData::QH1] = new TH1F("QH1", "QH1", 1024, 0, 1024);
         telHistograms[DAQ::FzData::I1] = new TH1F("I1", "I1", 500, 0, 500);
         telHistograms[DAQ::FzData::QL1] = new TH1F("QL1", "QL1", 1024, 0, 1024);
         telHistograms[DAQ::FzData::Q2] = new TH1F("Q2", "Q2", 1024, 0, 1024);
         telHistograms[DAQ::FzData::I2] = new TH1F("I2", "I2", 500, 0, 500);
         telHistograms[DAQ::FzData::Q3] = new TH1F("Q3", "Q3", 1024, 0, 1024);

         // create the sub pads:
         theCanvas->Divide(2, 3);

         // draw the histograms once
         for (int i=0; i<TEL_HIST_NUM; i++) { 

            theCanvas->cd(i+1);
            telHistograms[i]->SetMaximum(8500);
      	    telHistograms[i]->SetMinimum(-8500);
            telHistograms[i]->Draw();
         } 

         telHistograms[DAQ::FzData::QH1]->SetMaximum(8000);
         telHistograms[DAQ::FzData::QH1]->SetMinimum(-8000);
         telHistograms[DAQ::FzData::QL1]->SetMaximum(8000);
         telHistograms[DAQ::FzData::QL1]->SetMinimum(-8000); 
      }
   }

   theCanvas->cd(0);
   theCanvas->Update(); 

   // create two threads for filling and displsaying telHistograms[i].
   if ((connecthistogramfill(&zmq_sub)) == 0)
      printf("successfully started thread to fill histogram..\n");  
   if ((connectdisplayupdate(NULL)) == 0)
      printf("successfully started thread to update the display..\n");

   rootapp->Run();

   return 0; 	// never reached
}

