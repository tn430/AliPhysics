/*!
    \ingroup PWGPP
    \brief  ## Test of AliAnalysisTaskFiltered  class

    ## Macro to test functionality of the AliAnalysisTaskFiltered.
    To be used within UnitTest suit
    $AliPhysics_SRC/test/testAliAnalysisTaskFiltered/AliAnalysisTaskFilteredTest.sh
    To test:
        - 1.) CPU/Memory/Data volume
        -  2.) Relative fraction of the information in exported trees
        -  3.) Compression for points
    \author marian  Ivanov marian.ivanov@cern.ch
*/


void CheckOutput();

void AliAnalysisTaskFilteredTest(const char *esdList,
                                 Int_t run = 0,
                                 Float_t scalingTracks = 1,
                                 Float_t scalingV0 = 1,
                                 Float_t scalingFriend = 1,
                                 const char *ocdb = "cvmfs://",
                                 Int_t nFiles = 100000,
                                 Int_t firstFile = 0,
                                 Int_t nEvents = 1000000000,
                                 Int_t firstEvent = 0,
                                 Bool_t mc = kFALSE)
{
    TStopwatch timer;
    timer.Start();
 
    printf("\n\n\n");
    printf("scalingTracks=%d\n",scalingTracks);
    printf("scalingV0=%d\n",scalingV0);
    printf("nFiles=%d\n",nFiles);

    gSystem->SetIncludePath("-I$ROOTSYS/include -I$ALICE_ROOT/include -I$ALICE_ROOT/ITS -I$ALICE_ROOT -I$ALICE_ROOT/TRD");

    //____________________________________________//
    // Make the analysis manager
    AliAnalysisManager *mgr = new AliAnalysisManager("TestManager");
    mgr->SetDebugLevel(0);

    AliESDInputHandler* esdH = new AliESDInputHandler();
    //esdH->SetReadFriends(1);
    esdH->SetReadFriends(1);
    mgr->SetInputEventHandler(esdH);  

    // Enable MC event handler
    AliMCEventHandler* handlerMC = new AliMCEventHandler;
    //handler->SetReadTR(kFALSE);
    if (mc) mgr->SetMCtruthEventHandler(handlerMC);

    gROOT->LoadMacro("$ALICE_PHYSICS/PWGPP/PilotTrain/AddTaskCDBconnect.C");
    AddTaskCDBconnect(ocdb,run);

    if (gSystem->AccessPathName("localOCDBaccessConfig.C", kFileExists)==0) {
      gROOT->LoadMacro("localOCDBaccessConfig.C");
      localOCDBaccessConfig();
    }
    // Create input chain
    TChain* chain = AliXRDPROOFtoolkit::MakeChain(esdList, "esdTree",0,nFiles,firstFile);

    if(!chain) {
        printf("ERROR: chain cannot be created\n");
        return;
    }
    chain->Lookup();
  

    gROOT->LoadMacro("$ALICE_ROOT/ANALYSIS/macros/AddTaskPIDResponse.C");
    Bool_t isMC=kFALSE; // kTRUE in case of MC
    AddTaskPIDResponse(isMC); 

    //
    // Wagons to run 
    //
    gROOT->LoadMacro("$ALICE_PHYSICS/PWGPP/macros/AddTaskFilteredTree.C");
    AliAnalysisTaskFilteredTree* task = (AliAnalysisTaskFilteredTree*)AddTaskFilteredTree("Filtered.root");
    task->SetLowPtTrackDownscaligF(scalingTracks);
    task->SetLowPtV0DownscaligF(scalingV0);
    task->SetFriendDownscaling(scalingFriend);
    task->SetUseESDfriends(kTRUE);
    task->Dump();
    // Init
    if (!mgr->InitAnalysis()) 
        mgr->PrintStatus();
    //
    // Run on dataset
    mgr->StartAnalysis("local",chain,nEvents, firstEvent);
    timer.Stop();
    timer.Print();
    delete mgr;
    CheckOutput();
}


void CheckOutput(){
  //
  //
  //
  TFile * f  = TFile::Open("Filtered.root");
  TTree * highPt = (TTree*)f->Get("highPt");
  TTree * treeV0s = (TTree*)f->Get("V0s");
  //
  // Export variable:
  //
  Double_t ratioHighPtV0Entries=treeV0s->GetEntries()/Double_t(treeV0s->GetEntries()+highPt->GetEntries()+0.000001);
  Double_t ratioHighPtV0Size=treeV0s->GetZipBytes()/Double_t(treeV0s->GetZipBytes()+highPt->GetZipBytes()+0.000001);
  printf("#UnitTest:\tAliAnalysisTaskFiltered\tRatioPtToV0Entries\t%f\n",ratioHighPtV0Entries);
  printf("#UnitTest:\tAliAnalysisTaskFiltered\tRatioPtToV0Size\t%f\n",ratioHighPtV0Size);
  //
  //
  Double_t ratioPointsV0 = 2*treeV0s->GetBranch("friendTrack0.fCalibContainer")->GetZipBytes()/Double_t(0.00001+treeV0s->GetZipBytes());
  Double_t ratioPointsHighPt = highPt->GetBranch("friendTrack.fCalibContainer")->GetZipBytes()/Double_t(0.00001+highPt->GetZipBytes());
  printf("#UnitTest:\tAliAnalysisTaskFiltered\tRatioPointsV0\t%f\n",ratioPointsV0);
  printf("#UnitTest:\tAliAnalysisTaskFiltered\tRatioPointsHighPt\t%f\n",ratioPointsHighPt);
  //
  // a.) Check track correspondence
  //
  Int_t entries= highPt->Draw("(friendTrack.fTPCOut.fP[3]-esdTrack.fIp.fP[3])/sqrt(friendTrack.fTPCOut.fC[9]+esdTrack.fIp.fC[9])","friendTrack.fTPCOut.fP[3]!=0","");
  // here we should check if the tracks
  Double_t pulls=TMath::RMS(entries, highPt->GetV1());
  printf("#UnitTest:\tAliAnalysisTaskFiltered\tFriendPull\t%2.4f\n",pulls);
  printf("#UnitTest:\tAliAnalysisTaskFiltered\tFriendOK\t%d\n",pulls<10);

}
