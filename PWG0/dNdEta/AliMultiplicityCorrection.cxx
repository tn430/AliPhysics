/**************************************************************************
 * Copyright(c) 1998-1999, ALICE Experiment at CERN, All rights reserved. *
 *                                                                        *
 * Author: The ALICE Off-line Project.                                    *
 * Contributors are mentioned in the code where appropriate.              *
 *                                                                        *
 * Permission to use, copy, modify and distribute this software and its   *
 * documentation strictly for non-commercial purposes is hereby granted   *
 * without fee, provided that the above copyright notice appears in all   *
 * copies and that both the copyright notice and this permission notice   *
 * appear in the supporting documentation. The authors make no claims     *
 * about the suitability of this software for any purpose. It is          *
 * provided "as is" without express or implied warranty.                  *
 **************************************************************************/

/* $Id$ */

// This class is used to store correction maps, raw input and results of the multiplicity
// measurement with the ITS or TPC
// It also contains functions to correct the spectrum using different methods.
//
//  Author: Jan.Fiete.Grosse-Oetringhaus@cern.ch

#include "AliMultiplicityCorrection.h"

#include <TFile.h>
#include <TH1F.h>
#include <TH2F.h>
#include <TH3F.h>
#include <TDirectory.h>
#include <TVirtualFitter.h>
#include <TCanvas.h>
#include <TString.h>
#include <TF1.h>
#include <TMath.h>
#include <TCollection.h>
#include <TLegend.h>
#include <TLine.h>

ClassImp(AliMultiplicityCorrection)

const Int_t AliMultiplicityCorrection::fgMaxInput  = 250;  // bins in measured histogram
const Int_t AliMultiplicityCorrection::fgMaxParams = 250;  // bins in unfolded histogram = number of fit params

TH1* AliMultiplicityCorrection::fCurrentESD = 0;
TH1* AliMultiplicityCorrection::fCurrentCorrelation = 0;
TH1* AliMultiplicityCorrection::fCurrentEfficiency = 0;
TMatrixD* AliMultiplicityCorrection::fCorrelationMatrix = 0;
TMatrixD* AliMultiplicityCorrection::fCorrelationCovarianceMatrix = 0;
TVectorD* AliMultiplicityCorrection::fCurrentESDVector = 0;
TVectorD* AliMultiplicityCorrection::fEntropyAPriori = 0;
AliMultiplicityCorrection::RegularizationType AliMultiplicityCorrection::fRegularizationType = AliMultiplicityCorrection::kPol1;
Float_t AliMultiplicityCorrection::fRegularizationWeight = 5000;
TF1* AliMultiplicityCorrection::fNBD = 0;

//____________________________________________________________________
AliMultiplicityCorrection::AliMultiplicityCorrection() :
  TNamed(), fLastChi2MC(0), fLastChi2MCLimit(0), fLastChi2Residuals(0)
{
  //
  // default constructor
  //

  for (Int_t i = 0; i < kESDHists; ++i)
    fMultiplicityESD[i] = 0;

  for (Int_t i = 0; i < kMCHists; ++i)
  {
    fMultiplicityVtx[i] = 0;
    fMultiplicityMB[i] = 0;
    fMultiplicityINEL[i] = 0;
  }

  for (Int_t i = 0; i < kCorrHists; ++i)
  {
    fCorrelation[i] = 0;
    fMultiplicityESDCorrected[i] = 0;
  }
}

//____________________________________________________________________
AliMultiplicityCorrection::AliMultiplicityCorrection(const Char_t* name, const Char_t* title) :
  TNamed(name, title), fLastChi2MC(0), fLastChi2MCLimit(0), fLastChi2Residuals(0)
{
  //
  // named constructor
  //

  // do not add this hists to the directory
  Bool_t oldStatus = TH1::AddDirectoryStatus();
  TH1::AddDirectory(kFALSE);

  /*Float_t binLimitsVtx[] = {-10,-9,-8,-7,-6,-5,-4,-3,-2,-1,0,1,2,3,4,5,6,7,8,9,10};
  Float_t binLimitsN[] = {-0.5, 0.5, 1.5, 2.5, 3.5, 4.5, 5.5, 6.5, 7.5, 8.5, 9.5,
                          10.5, 11.5, 12.5, 13.5, 14.5, 15.5, 16.5, 17.5, 18.5, 19.5,
                          20.5, 21.5, 22.5, 23.5, 24.5, 25.5, 26.5, 27.5, 28.5, 29.5,
                          30.5, 31.5, 32.5, 33.5, 34.5, 35.5, 36.5, 37.5, 38.5, 39.5,
                          40.5, 41.5, 42.5, 43.5, 44.5, 45.5, 46.5, 47.5, 48.5, 49.5,
                          50.5, 55.5, 60.5, 65.5, 70.5, 75.5, 80.5, 85.5, 90.5, 95.5,
                          100.5, 105.5, 110.5, 115.5, 120.5, 125.5, 130.5, 135.5, 140.5, 145.5,
                          150.5, 160.5, 170.5, 180.5, 190.5, 200.5, 210.5, 220.5, 230.5, 240.5,
                          250.5, 275.5, 300.5, 325.5, 350.5, 375.5, 400.5, 425.5, 450.5, 475.5,
                          500.5 };
                                //525.5, 550.5, 575.5, 600.5, 625.5, 650.5, 675.5, 700.5, 725.5,
                          //750.5, 775.5, 800.5, 825.5, 850.5, 875.5, 900.5, 925.5, 950.5, 975.5,
                          //1000.5 };

  #define VTXBINNING 10, binLimitsVtx
  #define NBINNING fgMaxParams, binLimitsN*/

  #define NBINNING 501, -0.5, 500.5
  #define VTXBINNING 1, -10, 10

  for (Int_t i = 0; i < kESDHists; ++i)
    fMultiplicityESD[i] = new TH2F(Form("fMultiplicityESD%d", i), "fMultiplicityESD;vtx-z;Ntracks;Count", VTXBINNING, NBINNING);

  for (Int_t i = 0; i < kMCHists; ++i)
  {
    fMultiplicityVtx[i] = dynamic_cast<TH2F*> (fMultiplicityESD[0]->Clone(Form("fMultiplicityVtx%d", i)));
    fMultiplicityVtx[i]->SetTitle("fMultiplicityVtx;vtx-z;Npart");

    fMultiplicityMB[i] = dynamic_cast<TH2F*> (fMultiplicityVtx[0]->Clone(Form("fMultiplicityMB%d", i)));
    fMultiplicityMB[i]->SetTitle("fMultiplicityMB");

    fMultiplicityINEL[i] = dynamic_cast<TH2F*> (fMultiplicityVtx[0]->Clone(Form("fMultiplicityINEL%d", i)));
    fMultiplicityINEL[i]->SetTitle("fMultiplicityINEL");
  }

  for (Int_t i = 0; i < kCorrHists; ++i)
  {
    fCorrelation[i] = new TH3F(Form("fCorrelation%d", i), "fCorrelation;vtx-z;Npart;Ntracks", VTXBINNING, NBINNING, NBINNING);
    fMultiplicityESDCorrected[i] = new TH1F(Form("fMultiplicityESDCorrected%d", i), "fMultiplicityESDCorrected;Npart;dN/dN", NBINNING);
  }

  TH1::AddDirectory(oldStatus);
}

//____________________________________________________________________
AliMultiplicityCorrection::~AliMultiplicityCorrection()
{
  //
  // Destructor
  //
}

//____________________________________________________________________
Long64_t AliMultiplicityCorrection::Merge(TCollection* list)
{
  // Merge a list of AliMultiplicityCorrection objects with this (needed for
  // PROOF).
  // Returns the number of merged objects (including this).

  if (!list)
    return 0;

  if (list->IsEmpty())
    return 1;

  TIterator* iter = list->MakeIterator();
  TObject* obj;

  // collections of all histograms
  TList collections[kESDHists+kMCHists*3+kCorrHists*2];

  Int_t count = 0;
  while ((obj = iter->Next())) {

    AliMultiplicityCorrection* entry = dynamic_cast<AliMultiplicityCorrection*> (obj);
    if (entry == 0) 
      continue;

    for (Int_t i = 0; i < kESDHists; ++i)
      collections[i].Add(entry->fMultiplicityESD[i]);

    for (Int_t i = 0; i < kMCHists; ++i)
    {
      collections[kESDHists+i].Add(entry->fMultiplicityVtx[i]);
      collections[kESDHists+kMCHists+i].Add(entry->fMultiplicityMB[i]);
      collections[kESDHists+kMCHists*2+i].Add(entry->fMultiplicityINEL[i]);
    }

    for (Int_t i = 0; i < kCorrHists; ++i)
      collections[kESDHists+kMCHists*3+i].Add(entry->fCorrelation[i]);

    for (Int_t i = 0; i < kCorrHists; ++i)
      collections[kESDHists+kMCHists*3+kCorrHists+i].Add(entry->fMultiplicityESDCorrected[i]);

    count++;
  }

  for (Int_t i = 0; i < kESDHists; ++i)
    fMultiplicityESD[i]->Merge(&collections[i]);

  for (Int_t i = 0; i < kMCHists; ++i)
  {
    fMultiplicityVtx[i]->Merge(&collections[kESDHists+i]);
    fMultiplicityMB[i]->Merge(&collections[kESDHists+kMCHists+i]);
    fMultiplicityINEL[i]->Merge(&collections[kESDHists+kMCHists*2+i]);
  }

  for (Int_t i = 0; i < kCorrHists; ++i)
    fCorrelation[i]->Merge(&collections[kESDHists+kMCHists*3+i]);

  for (Int_t i = 0; i < kCorrHists; ++i)
    fMultiplicityESDCorrected[i]->Merge(&collections[kESDHists+kMCHists*3+kCorrHists+i]);

  delete iter;

  return count+1;
}

//____________________________________________________________________
Bool_t AliMultiplicityCorrection::LoadHistograms(const Char_t* dir)
{
  //
  // loads the histograms from a file
  // if dir is empty a directory with the name of this object is taken (like in SaveHistogram)
  //

  if (!dir)
    dir = GetName();

  if (!gDirectory->cd(dir))
    return kFALSE;

  // TODO memory leak. old histograms needs to be deleted.

  Bool_t success = kTRUE;

  for (Int_t i = 0; i < kESDHists; ++i)
  {
    fMultiplicityESD[i] = dynamic_cast<TH2F*> (gDirectory->Get(fMultiplicityESD[i]->GetName()));
    if (!fMultiplicityESD[i])
      success = kFALSE;
  }

  for (Int_t i = 0; i < kMCHists; ++i)
  {
    fMultiplicityVtx[i] = dynamic_cast<TH2F*> (gDirectory->Get(fMultiplicityVtx[i]->GetName()));
    fMultiplicityMB[i] = dynamic_cast<TH2F*> (gDirectory->Get(fMultiplicityMB[i]->GetName()));
    fMultiplicityINEL[i] = dynamic_cast<TH2F*> (gDirectory->Get(fMultiplicityINEL[i]->GetName()));
    if (!fMultiplicityVtx[i] || !fMultiplicityMB[i] || !fMultiplicityINEL[i])
      success = kFALSE;
  }

  for (Int_t i = 0; i < kCorrHists; ++i)
  {
    fCorrelation[i] = dynamic_cast<TH3F*> (gDirectory->Get(fCorrelation[i]->GetName()));
    if (!fCorrelation[i])
      success = kFALSE;
    fMultiplicityESDCorrected[i] = dynamic_cast<TH1F*> (gDirectory->Get(fMultiplicityESDCorrected[i]->GetName()));
    if (!fMultiplicityESDCorrected[i])
      success = kFALSE;
  }

  gDirectory->cd("..");

  return success;
}

//____________________________________________________________________
void AliMultiplicityCorrection::SaveHistograms()
{
  //
  // saves the histograms
  //

  gDirectory->mkdir(GetName());
  gDirectory->cd(GetName());

  for (Int_t i = 0; i < kESDHists; ++i)
    if (fMultiplicityESD[i])
      fMultiplicityESD[i]->Write();

  for (Int_t i = 0; i < kMCHists; ++i)
  {
    if (fMultiplicityVtx[i])
      fMultiplicityVtx[i]->Write();
    if (fMultiplicityMB[i])
      fMultiplicityMB[i]->Write();
    if (fMultiplicityINEL[i])
      fMultiplicityINEL[i]->Write();
  }

  for (Int_t i = 0; i < kCorrHists; ++i)
  {
    if (fCorrelation[i])
      fCorrelation[i]->Write();
    if (fMultiplicityESDCorrected[i])
      fMultiplicityESDCorrected[i]->Write();
  }

  gDirectory->cd("..");
}

//____________________________________________________________________
void AliMultiplicityCorrection::FillGenerated(Float_t vtx, Bool_t triggered, Bool_t vertex, Int_t generated05, Int_t generated10, Int_t generated15, Int_t generated20, Int_t generatedAll)
{
  //
  // Fills an event from MC
  //

  if (triggered)
  {
    fMultiplicityMB[0]->Fill(vtx, generated05);
    fMultiplicityMB[1]->Fill(vtx, generated10);
    fMultiplicityMB[2]->Fill(vtx, generated15);
    fMultiplicityMB[3]->Fill(vtx, generated20);
    fMultiplicityMB[4]->Fill(vtx, generatedAll);

    if (vertex)
    {
      fMultiplicityVtx[0]->Fill(vtx, generated05);
      fMultiplicityVtx[1]->Fill(vtx, generated10);
      fMultiplicityVtx[2]->Fill(vtx, generated15);
      fMultiplicityVtx[3]->Fill(vtx, generated20);
      fMultiplicityVtx[4]->Fill(vtx, generatedAll);
    }
  }

  fMultiplicityINEL[0]->Fill(vtx, generated05);
  fMultiplicityINEL[1]->Fill(vtx, generated10);
  fMultiplicityINEL[2]->Fill(vtx, generated15);
  fMultiplicityINEL[3]->Fill(vtx, generated20);
  fMultiplicityINEL[4]->Fill(vtx, generatedAll);
}

//____________________________________________________________________
void AliMultiplicityCorrection::FillMeasured(Float_t vtx, Int_t measured05, Int_t measured10, Int_t measured15, Int_t measured20)
{
  //
  // Fills an event from ESD
  //

  fMultiplicityESD[0]->Fill(vtx, measured05);
  fMultiplicityESD[1]->Fill(vtx, measured10);
  fMultiplicityESD[2]->Fill(vtx, measured15);
  fMultiplicityESD[3]->Fill(vtx, measured20);
}

//____________________________________________________________________
void AliMultiplicityCorrection::FillCorrection(Float_t vtx, Int_t generated05, Int_t generated10, Int_t generated15, Int_t generated20, Int_t generatedAll, Int_t measured05, Int_t measured10, Int_t measured15, Int_t measured20)
{
  //
  // Fills an event into the correlation map with the information from MC and ESD
  //

  fCorrelation[0]->Fill(vtx, generated05, measured05);
  fCorrelation[1]->Fill(vtx, generated10, measured10);
  fCorrelation[2]->Fill(vtx, generated15, measured15);
  fCorrelation[3]->Fill(vtx, generated20, measured20);

  fCorrelation[4]->Fill(vtx, generatedAll, measured05);
  fCorrelation[5]->Fill(vtx, generatedAll, measured10);
  fCorrelation[6]->Fill(vtx, generatedAll, measured15);
  fCorrelation[7]->Fill(vtx, generatedAll, measured20);
}

//____________________________________________________________________
Double_t AliMultiplicityCorrection::RegularizationPol0(TVectorD& params)
{
  // homogenity term for minuit fitting
  // pure function of the parameters
  // prefers constant function (pol0)

  Double_t chi2 = 0;

  // ignore the first bin here. on purpose...
  for (Int_t i=2; i<fgMaxParams; ++i)
  {
    Double_t right  = params[i];
    Double_t left   = params[i-1];

    if (right != 0)
    {
      Double_t diff = 1 - left / right;
      chi2 += diff * diff;
    }
  }

  return chi2 / 100.0;
}

//____________________________________________________________________
Double_t AliMultiplicityCorrection::RegularizationPol1(TVectorD& params)
{
  // homogenity term for minuit fitting
  // pure function of the parameters
  // prefers linear function (pol1)

  Double_t chi2 = 0;

  // ignore the first bin here. on purpose...
  for (Int_t i=2+1; i<fgMaxParams; ++i)
  {
    if (params[i-1] == 0)
      continue;

    Double_t right  = params[i];
    Double_t middle = params[i-1];
    Double_t left   = params[i-2];

    Double_t der1 = (right - middle);
    Double_t der2 = (middle - left);

    Double_t diff = (der1 - der2) / middle;

    chi2 += diff * diff;
  }

  return chi2;
}

//____________________________________________________________________
Double_t AliMultiplicityCorrection::RegularizationTest(TVectorD& params)
{
  // homogenity term for minuit fitting
  // pure function of the parameters
  // prefers linear function (pol1)

  Double_t chi2 = 0;

  Float_t der[fgMaxParams];

  for (Int_t i=0; i<fgMaxParams-1; ++i)
    der[i] = params[i+1] - params[i];

  for (Int_t i=0; i<fgMaxParams-6; ++i)
  {
    for (Int_t j=1; j<=5; ++j)
    {
      Double_t diff = der[i+j] - der[i];
      chi2 += diff * diff;
    }
  }

  return chi2 * 100; // 10000
}

//____________________________________________________________________
Double_t AliMultiplicityCorrection::RegularizationTotalCurvature(TVectorD& params)
{
  // homogenity term for minuit fitting
  // pure function of the parameters
  // minimizes the total curvature (from Unfolding Methods In High-Energy Physics Experiments,
  // V. Blobel (Hamburg U.) . DESY 84/118, Dec 1984. 40pp.

  Double_t chi2 = 0;

  // ignore the first bin here. on purpose...
  for (Int_t i=3; i<fgMaxParams; ++i)
  {
    Double_t right  = params[i];
    Double_t middle = params[i-1];
    Double_t left   = params[i-2];

    Double_t der1 = (right - middle);
    Double_t der2 = (middle - left);

    Double_t diff = (der1 - der2);

    chi2 += diff * diff;
  }

  return chi2 * 1e4;
}

//____________________________________________________________________
Double_t AliMultiplicityCorrection::RegularizationEntropy(TVectorD& params)
{
  // homogenity term for minuit fitting
  // pure function of the parameters
  // calculates entropy, from
  // The method of reduced cross-entropy (M. Schmelling 1993)

  Double_t paramSum = 0;
  // ignore the first bin here. on purpose...
  for (Int_t i=1; i<fgMaxParams; ++i)
    paramSum += params[i];

  Double_t chi2 = 0;
  for (Int_t i=1; i<fgMaxParams; ++i)
  {
    Double_t tmp = params[i] / paramSum;
    if (tmp > 0 && (*fEntropyAPriori)[i] > 0)
      chi2 += tmp * log(tmp / (*fEntropyAPriori)[i]);
  }

  return 10.0 + chi2;
}

//____________________________________________________________________
void AliMultiplicityCorrection::MinuitNBD(Int_t& unused1, Double_t* unused2, Double_t& chi2, Double_t *params, Int_t unused3)
{
  //
  // fit function for minuit
  // does: nbd
  // func = new TF1("nbd", "[0] * TMath::Binomial([2]+TMath::Nint(x)-1, [2]-1) * pow([1] / ([1]+[2]), TMath::Nint(x)) * pow(1 + [1]/[2], -[2])", 0, 50);
  // func->SetParNames("scaling", "averagen", "k");
  // func->SetParLimits(0, 0.001, fCurrentESD->GetMaximum() * 1000);
  // func->SetParLimits(1, 0.001, 1000);
  // func->SetParLimits(2, 0.001, 1000);
  //

  fNBD->SetParameters(params[0], params[1], params[2]);

  Double_t params2[fgMaxParams];

  for (Int_t i=0; i<fgMaxParams; ++i)
    params2[i] = fNBD->Eval(i);

  MinuitFitFunction(unused1, unused2, chi2, params2, unused3);

  printf("%f %f %f --> %f\n", params[0], params[1], params[2], chi2);
}

//____________________________________________________________________
void AliMultiplicityCorrection::MinuitFitFunction(Int_t&, Double_t*, Double_t& chi2, Double_t *params, Int_t)
{
  //
  // fit function for minuit
  // does: (m - Ad)W(m - Ad) where m = measured, A correlation matrix, d = guess, W = covariance matrix
  //

  // d
  TVectorD paramsVector(fgMaxParams);
  for (Int_t i=0; i<fgMaxParams; ++i)
    paramsVector[i] = params[i] * params[i];

  // calculate penalty factor
  Double_t penaltyVal = 0;
  switch (fRegularizationType)
  {
    case kNone:       break;
    case kPol0:       penaltyVal = RegularizationPol0(paramsVector); break;
    case kPol1:       penaltyVal = RegularizationPol1(paramsVector); break;
    case kCurvature:  penaltyVal = RegularizationTotalCurvature(paramsVector); break;
    case kEntropy:    penaltyVal = RegularizationEntropy(paramsVector); break;
    case kTest:       penaltyVal = RegularizationTest(paramsVector); break;
  }

  //if (penaltyVal > 1e10)
  //  paramsVector2.Print();

  penaltyVal *= fRegularizationWeight;

  // Ad
  TVectorD measGuessVector(fgMaxInput);
  measGuessVector = (*fCorrelationMatrix) * paramsVector;

  // Ad - m
  measGuessVector -= (*fCurrentESDVector);

  TVectorD copy(measGuessVector);

  // (Ad - m) W
  measGuessVector *= (*fCorrelationCovarianceMatrix);

  //measGuessVector.Print();

  // (Ad - m) W (Ad - m)
  Double_t chi2FromFit = measGuessVector * copy * 1e6;

  chi2 = chi2FromFit + penaltyVal;

  static Int_t callCount = 0;
  if ((callCount++ % 10000) == 0)
    printf("%d) %f %f --> %f\n", callCount, chi2FromFit, penaltyVal, chi2);
}

//____________________________________________________________________
void AliMultiplicityCorrection::SetupCurrentHists(Int_t inputRange, Bool_t fullPhaseSpace, EventType eventType, Bool_t createBigBin)
{
  //
  // fills fCurrentESD, fCurrentCorrelation
  // resets fMultiplicityESDCorrected
  //

  Int_t correlationID = inputRange + ((fullPhaseSpace == kFALSE) ? 0 : 4);
  fMultiplicityESDCorrected[correlationID]->Reset();

  fCurrentESD = fMultiplicityESD[inputRange]->ProjectionY();
  fCurrentESD->Sumw2();

  // empty under/overflow bins in x, otherwise Project3D takes them into account
  TH3* hist = fCorrelation[correlationID];
  for (Int_t y=1; y<=hist->GetYaxis()->GetNbins(); ++y)
  {
    for (Int_t z=1; z<=hist->GetZaxis()->GetNbins(); ++z)
    {
      hist->SetBinContent(0, y, z, 0);
      hist->SetBinContent(hist->GetXaxis()->GetNbins()+1, y, z, 0);
    }
  }

  fCurrentCorrelation = hist->Project3D("zy");
  //((TH2*) fCurrentCorrelation)->Rebin2D(2, 1);
  //fMultiplicityESDCorrected[correlationID]->Rebin(2);
  fCurrentCorrelation->Sumw2();

  if (createBigBin)
  {
    Int_t maxBin = 0;
    for (Int_t i=1; i<=fCurrentESD->GetNbinsX(); ++i)
    {
      if (fCurrentESD->GetBinContent(i) <= 5)
      {
        maxBin = i;
        break;
      }
    }

    if (maxBin > 0)
    {
      TCanvas* canvas = new TCanvas("StatSolution", "StatSolution", 1000, 800);
      canvas->Divide(2, 2);

      canvas->cd(1);
      fCurrentESD->DrawCopy();
      gPad->SetLogy();

      canvas->cd(2);
      fCurrentCorrelation->DrawCopy("COLZ");

      printf("Bin limit in measured spectrum is %d.\n", maxBin);
      fCurrentESD->SetBinContent(maxBin, fCurrentESD->Integral(maxBin, fCurrentESD->GetNbinsX()));
      for (Int_t i=maxBin+1; i<=fCurrentESD->GetNbinsX(); ++i)
      {
        fCurrentESD->SetBinContent(i, 0);
        fCurrentESD->SetBinError(i, 0);
      }
      // the error is set to sqrt(N), better solution possible?, sum of relative errors of all contributions???
      fCurrentESD->SetBinError(maxBin, TMath::Sqrt(fCurrentESD->GetBinContent(maxBin)));

      printf("This bin has now %f +- %f entries\n", fCurrentESD->GetBinContent(maxBin), fCurrentESD->GetBinError(maxBin));

      for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
      {
        fCurrentCorrelation->SetBinContent(i, maxBin, fCurrentCorrelation->Integral(i, i, maxBin, fCurrentCorrelation->GetNbinsY()));
        // the error is set to sqrt(N), better solution possible?, sum of relative errors of all contributions???
        fCurrentCorrelation->SetBinError(i, maxBin, TMath::Sqrt(fCurrentCorrelation->GetBinContent(i, maxBin)));

        for (Int_t j=maxBin+1; j<=fCurrentCorrelation->GetNbinsY(); ++j)
        {
          fCurrentCorrelation->SetBinContent(i, j, 0);
          fCurrentCorrelation->SetBinError(i, j, 0);
        }
      }

      printf("Adjusted correlation matrix!\n");

      canvas->cd(3);
      fCurrentESD->DrawCopy();
      gPad->SetLogy();

      canvas->cd(4);
      fCurrentCorrelation->DrawCopy("COLZ");
    }
  }

  //normalize ESD
  fCurrentESD->Scale(1.0 / fCurrentESD->Integral());

  fCurrentEfficiency = fMultiplicityVtx[inputRange]->ProjectionY("CurrentEfficiency");
  TH2* divisor = 0;
  switch (eventType)
  {
    case kTrVtx : divisor = fMultiplicityVtx[inputRange]; break;
    case kMB: divisor = fMultiplicityMB[inputRange]; break;
    case kINEL: divisor = fMultiplicityINEL[inputRange]; break;
  }
  fCurrentEfficiency->Divide(divisor->ProjectionY());
  //fCurrentEfficiency->Rebin(2);
  //fCurrentEfficiency->Scale(0.5);
}

//____________________________________________________________________
void AliMultiplicityCorrection::SetRegularizationParameters(RegularizationType type, Float_t weight)
{
  fRegularizationType = type;
  fRegularizationWeight = weight;

  printf("AliMultiplicityCorrection::SetRegularizationParameters --> Regularization set to %d with weight %f\n", (Int_t) type, weight);
}

//____________________________________________________________________
Int_t AliMultiplicityCorrection::ApplyMinuitFit(Int_t inputRange, Bool_t fullPhaseSpace, EventType eventType, Bool_t check, TH1* inputDist)
{
  //
  // correct spectrum using minuit chi2 method
  //
  // if check is kTRUE the input MC solution (by definition the right solution) is used
  // no fit is made and just the chi2 is calculated
  //

  Int_t correlationID = inputRange + ((fullPhaseSpace == kFALSE) ? 0 : 4);
  Int_t mcTarget = ((fullPhaseSpace == kFALSE) ? inputRange : 4);

  SetupCurrentHists(inputRange, fullPhaseSpace, eventType, kFALSE); // TODO FAKE kTRUE

  fCorrelationMatrix = new TMatrixD(fgMaxInput, fgMaxParams);
  fCorrelationCovarianceMatrix = new TMatrixD(fgMaxInput, fgMaxInput);
  fCurrentESDVector = new TVectorD(fgMaxInput);
  fEntropyAPriori = new TVectorD(fgMaxParams);

  /*new TCanvas; fCurrentESD->DrawCopy();
  fCurrentESD = ((TH2*) fCurrentCorrelation)->ProjectionY("check-proj2");
  fCurrentESD->Sumw2();
  fCurrentESD->Scale(1.0 / fCurrentESD->Integral());
  fCurrentESD->SetLineColor(2);
  fCurrentESD->DrawCopy("SAME");*/

  // normalize correction for given nPart
  for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
  {
    Double_t sum = fCurrentCorrelation->Integral(i, i, 1, fCurrentCorrelation->GetNbinsY());
    if (sum <= 0)
      continue;
    Float_t maxValue = 0;
    Int_t maxBin = -1;
    for (Int_t j=1; j<=fCurrentCorrelation->GetNbinsY(); ++j)
    {
      // find most probably value
      if (maxValue < fCurrentCorrelation->GetBinContent(i, j))
      {
        maxValue = fCurrentCorrelation->GetBinContent(i, j);
        maxBin = j;
      }

      // npart sum to 1
      fCurrentCorrelation->SetBinContent(i, j, fCurrentCorrelation->GetBinContent(i, j) / sum);
      fCurrentCorrelation->SetBinError(i, j, fCurrentCorrelation->GetBinError(i, j) / sum);

      if (i <= fgMaxParams && j <= fgMaxInput)
        (*fCorrelationMatrix)(j-1, i-1) = fCurrentCorrelation->GetBinContent(i, j);
    }

    //printf("MPV for Ntrue = %f is %f\n", fCurrentCorrelation->GetXaxis()->GetBinCenter(i), fCurrentCorrelation->GetYaxis()->GetBinCenter(maxBin));
  }

  // Initialize TMinuit via generic fitter interface
  TVirtualFitter *minuit = TVirtualFitter::Fitter(0, fgMaxParams);
  Double_t arglist[100];
  arglist[0] = 0;
  minuit->ExecuteCommand("SET PRINT", arglist, 1);

  minuit->SetFCN(MinuitFitFunction);

  for (Int_t i=0; i<fgMaxParams; i++)
    (*fEntropyAPriori)[i] = 1;

  if (inputDist)
  {
    printf("Using different starting conditions...\n");
    new TCanvas;
    inputDist->DrawCopy();

    inputDist->Scale(1.0 / inputDist->Integral());
    for (Int_t i=0; i<fgMaxParams; i++)
      if (inputDist->GetBinContent(i+1) > 0)
        (*fEntropyAPriori)[i] = inputDist->GetBinContent(i+1);
  }
  else
    inputDist = fCurrentESD;


  //Float_t minStartValue = 0; //1e-3;

  //new TCanvas; fMultiplicityVtx[mcTarget]->Draw("COLZ");
  TH1* proj = fMultiplicityVtx[mcTarget]->ProjectionY("check-proj");
  //proj->Rebin(2);
  proj->Scale(1.0 / proj->Integral());

  Double_t results[fgMaxParams];
  for (Int_t i=0; i<fgMaxParams; ++i)
  {
    results[i] = inputDist->GetBinContent(i+1);

    if (check)
      results[i] = proj->GetBinContent(i+1);

    // minimum value
    results[i] = TMath::Max(results[i], 1e-3);

    Float_t stepSize = 0.1;

    // minuit sees squared values to prevent it from going negative...
    results[i] = TMath::Sqrt(results[i]);
    //stepSize /= results[i]; // keep relative step size

    minuit->SetParameter(i, Form("param%d", i), results[i], stepSize, 0, 0);
  }
  // bin 0 is filled with value from bin 1 (otherwise it's 0)
  //minuit->SetParameter(0, "param0", (results[1] > minStartValue) ? results[1] : minStartValue, 0.1, 0, 0);
  //results[0] = 0;
  //minuit->SetParameter(0, "param0", 0, 0, 0, 0);

  for (Int_t i=0; i<fgMaxInput; ++i)
  {
    //printf("%f %f %f\n", inputDist->GetBinContent(i+1), TMath::Sqrt(inputDist->GetBinContent(i+1)), inputDist->GetBinError(i+1));

    (*fCurrentESDVector)[i] = fCurrentESD->GetBinContent(i+1);
    if (fCurrentESD->GetBinError(i+1) > 0)
      (*fCorrelationCovarianceMatrix)(i, i) = (Double_t) 1e-6 / fCurrentESD->GetBinError(i+1) / fCurrentESD->GetBinError(i+1);

    if ((*fCorrelationCovarianceMatrix)(i, i) > 1e7)
      (*fCorrelationCovarianceMatrix)(i, i) = 0;

    //printf("%d --> %e\n", i, (*fCorrelationCovarianceMatrix)(i, i));
  }

  Int_t dummy;
  Double_t chi2;
  MinuitFitFunction(dummy, 0, chi2, results, 0);
  printf("Chi2 of initial parameters is = %f\n", chi2);

  if (check)
    return -1;

  // first param is number of iterations, second is precision....
  arglist[0] = 1e6;
  //arglist[1] = 1e-5;
  //minuit->ExecuteCommand("SCAN", arglist, 0);
  Int_t status = minuit->ExecuteCommand("MIGRAD", arglist, 1);
  printf("MINUIT status is %d\n", status);
  //minuit->ExecuteCommand("MIGRAD", arglist, 1);
  //minuit->ExecuteCommand("MIGRAD", arglist, 1);
  //printf("!!!!!!!!!!!!!! MIGRAD finished: Starting MINOS !!!!!!!!!!!!!!");
  //minuit->ExecuteCommand("MINOS", arglist, 0);

  for (Int_t i=0; i<fgMaxParams; ++i)
  {
    if (fCurrentEfficiency->GetBinContent(i+1) > 0)
    {
      fMultiplicityESDCorrected[correlationID]->SetBinContent(i+1, minuit->GetParameter(i) * minuit->GetParameter(i) / fCurrentEfficiency->GetBinContent(i+1));
      // error is : (relError) * (value) = (minuit->GetParError(i) / minuit->GetParameter(i)) * (minuit->GetParameter(i) * minuit->GetParameter(i))
      fMultiplicityESDCorrected[correlationID]->SetBinError(i+1, minuit->GetParError(i) * minuit->GetParameter(i) /  fCurrentEfficiency->GetBinContent(i+1));
    }
    else
    {
      fMultiplicityESDCorrected[correlationID]->SetBinContent(i+1, 0);
      fMultiplicityESDCorrected[correlationID]->SetBinError(i+1, 0);
    }
  }

  return status;
}

//____________________________________________________________________
void AliMultiplicityCorrection::ApplyNBDFit(Int_t inputRange, Bool_t fullPhaseSpace)
{
  //
  // correct spectrum using minuit chi2 method applying a NBD fit
  //

  Int_t correlationID = inputRange + ((fullPhaseSpace == kFALSE) ? 0 : 4);

  SetupCurrentHists(inputRange, fullPhaseSpace, kTrVtx, kFALSE);

  fCorrelationMatrix = new TMatrixD(fgMaxParams, fgMaxParams);
  fCorrelationCovarianceMatrix = new TMatrixD(fgMaxParams, fgMaxParams);
  fCurrentESDVector = new TVectorD(fgMaxParams);

  // normalize correction for given nPart
  for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
  {
    Double_t sum = fCurrentCorrelation->Integral(i, i, 1, fCurrentCorrelation->GetNbinsY());
    if (sum <= 0)
      continue;
    for (Int_t j=1; j<=fCurrentCorrelation->GetNbinsY(); ++j)
    {
      // npart sum to 1
      fCurrentCorrelation->SetBinContent(i, j, fCurrentCorrelation->GetBinContent(i, j) / sum);
      fCurrentCorrelation->SetBinError(i, j, fCurrentCorrelation->GetBinError(i, j) / sum);

      if (i <= fgMaxParams && j <= fgMaxParams)
        (*fCorrelationMatrix)(j-1, i-1) = fCurrentCorrelation->GetBinContent(i, j);
    }
  }

  for (Int_t i=0; i<fgMaxParams; ++i)
  {
    (*fCurrentESDVector)[i] = fCurrentESD->GetBinContent(i+1);
    if (fCurrentESD->GetBinError(i+1) > 0)
      (*fCorrelationCovarianceMatrix)(i, i) = 1.0 / (fCurrentESD->GetBinError(i+1) * fCurrentESD->GetBinError(i+1));
  }

  // Create NBD function
  if (!fNBD)
  {
    fNBD = new TF1("nbd", "[0] * TMath::Binomial([2]+TMath::Nint(x)-1, [2]-1) * pow([1] / ([1]+[2]), TMath::Nint(x)) * pow(1 + [1]/[2], -[2])", 0, 250);
    fNBD->SetParNames("scaling", "averagen", "k");
  }

  // Initialize TMinuit via generic fitter interface
  TVirtualFitter *minuit = TVirtualFitter::Fitter(0, 3);

  minuit->SetFCN(MinuitNBD);
  minuit->SetParameter(0, "param0", 1, 0.1, 0.001, fCurrentESD->GetMaximum() * 1000);
  minuit->SetParameter(1, "param1", 10, 1, 0.001, 1000);
  minuit->SetParameter(2, "param2", 2, 1, 0.001, 1000);

  Double_t arglist[100];
  arglist[0] = 0;
  minuit->ExecuteCommand("SET PRINT", arglist, 1);
  minuit->ExecuteCommand("MIGRAD", arglist, 0);
  //minuit->ExecuteCommand("MINOS", arglist, 0);

  fNBD->SetParameters(minuit->GetParameter(0), minuit->GetParameter(1), minuit->GetParameter(2));

  for (Int_t i=0; i<fgMaxParams; ++i)
  {
    printf("%d : %f\n", i, fNBD->Eval(i));
    if (fNBD->Eval(i) > 0)
    {
      fMultiplicityESDCorrected[correlationID]->SetBinContent(i+1, fNBD->Eval(i));
      fMultiplicityESDCorrected[correlationID]->SetBinError(i+1, 0);
    }
  }
}

//____________________________________________________________________
void AliMultiplicityCorrection::NormalizeToBinWidth(TH1* hist)
{
  //
  // normalizes a 1-d histogram to its bin width
  //

  for (Int_t i=1; i<=hist->GetNbinsX(); ++i)
  {
    hist->SetBinContent(i, hist->GetBinContent(i) / hist->GetBinWidth(i));
    hist->SetBinError(i, hist->GetBinError(i) / hist->GetBinWidth(i));
  }
}

//____________________________________________________________________
void AliMultiplicityCorrection::NormalizeToBinWidth(TH2* hist)
{
  //
  // normalizes a 2-d histogram to its bin width (x width * y width)
  //

  for (Int_t i=1; i<=hist->GetNbinsX(); ++i)
    for (Int_t j=1; j<=hist->GetNbinsY(); ++j)
    {
      Double_t factor = hist->GetXaxis()->GetBinWidth(i) * hist->GetYaxis()->GetBinWidth(j);
      hist->SetBinContent(i, j, hist->GetBinContent(i, j) / factor);
      hist->SetBinError(i, j, hist->GetBinError(i, j) / factor);
    }
}

//____________________________________________________________________
void AliMultiplicityCorrection::DrawHistograms()
{
  printf("ESD:\n");

  TCanvas* canvas1 = new TCanvas("fMultiplicityESD", "fMultiplicityESD", 900, 600);
  canvas1->Divide(3, 2);
  for (Int_t i = 0; i < kESDHists; ++i)
  {
    canvas1->cd(i+1);
    fMultiplicityESD[i]->DrawCopy("COLZ");
    printf("%d --> %f\n", i, (Float_t) fMultiplicityESD[i]->ProjectionY()->GetMean());
  }

  printf("Vtx:\n");

  TCanvas* canvas2 = new TCanvas("fMultiplicityMC", "fMultiplicityMC", 900, 600);
  canvas2->Divide(3, 2);
  for (Int_t i = 0; i < kMCHists; ++i)
  {
    canvas2->cd(i+1);
    fMultiplicityVtx[i]->DrawCopy("COLZ");
    printf("%d --> %f\n", i, fMultiplicityVtx[i]->ProjectionY()->GetMean());
  }

  TCanvas* canvas3 = new TCanvas("fCorrelation", "fCorrelation", 900, 900);
  canvas3->Divide(3, 3);
  for (Int_t i = 0; i < kCorrHists; ++i)
  {
    canvas3->cd(i+1);
    TH3* hist = dynamic_cast<TH3*> (fCorrelation[i]->Clone());
    for (Int_t y=1; y<=hist->GetYaxis()->GetNbins(); ++y)
    {
      for (Int_t z=1; z<=hist->GetZaxis()->GetNbins(); ++z)
      {
        hist->SetBinContent(0, y, z, 0);
        hist->SetBinContent(hist->GetXaxis()->GetNbins()+1, y, z, 0);
      }
    }
    TH1* proj = hist->Project3D("zy");
    proj->DrawCopy("COLZ");
  }
}

//____________________________________________________________________
void AliMultiplicityCorrection::DrawComparison(const char* name, Int_t inputRange, Bool_t fullPhaseSpace, Bool_t normalizeESD, TH1* mcHist, Bool_t simple)
{
  //mcHist->Rebin(2);

  Int_t esdCorrId = inputRange + ((fullPhaseSpace == kFALSE) ? 0 : 4);

  TString tmpStr;
  tmpStr.Form("%s_DrawComparison_%d", name, esdCorrId);

  if (fMultiplicityESDCorrected[esdCorrId]->Integral() == 0)
  {
    printf("ERROR. Unfolded histogram is empty\n");
    return;
  }

  //regain measured distribution used for unfolding, because the bins at high mult. were modified in SetupCurrentHists
  fCurrentESD = fMultiplicityESD[esdCorrId]->ProjectionY();
  fCurrentESD->Sumw2();
  fCurrentESD->Scale(1.0 / fCurrentESD->Integral());

  // normalize unfolded result to 1
  fMultiplicityESDCorrected[esdCorrId]->Scale(1.0 / fMultiplicityESDCorrected[esdCorrId]->Integral());

  //fCurrentESD->Scale(mcHist->Integral(2, 200));

  //new TCanvas;
  /*TH1* ratio = (TH1*) fMultiplicityESDCorrected[esdCorrId]->Clone("ratio");
  ratio->Divide(mcHist);
  ratio->Draw("HIST");
  ratio->Fit("pol0", "W0", "", 20, 120);
  Float_t scalingFactor = ratio->GetFunction("pol0")->GetParameter(0);
  delete ratio;
  mcHist->Scale(scalingFactor);*/

  mcHist->Scale(1.0 / mcHist->Integral());

  // calculate residual

  // for that we convolute the response matrix with the gathered result
  // if normalizeESD is not set, the histogram is already normalized, this needs to be passed to CalculateMultiplicityESD
  TH1* tmpESDEfficiencyRecorrected = (TH1*) fMultiplicityESDCorrected[esdCorrId]->Clone("tmpESDEfficiencyRecorrected");
  tmpESDEfficiencyRecorrected->Multiply(fCurrentEfficiency);
  TH2* convoluted = CalculateMultiplicityESD(tmpESDEfficiencyRecorrected, esdCorrId);
  TH1* convolutedProj = convoluted->ProjectionY("convolutedProj", -1, -1, "e");
  if (convolutedProj->Integral() > 0)
  {
    convolutedProj->Scale(1.0 / convolutedProj->Integral());
  }
  else
    printf("ERROR: convolutedProj is empty. Something went wrong calculating the convoluted histogram.\n");

  //NormalizeToBinWidth(proj2);

  TH1* residual = (TH1*) convolutedProj->Clone("residual");
  residual->SetTitle("Residuals;Ntracks;(folded unfolded measured - measured) / e");

  residual->Add(fCurrentESD, -1);
  //residual->Divide(residual, fCurrentESD, 1, 1, "B");

  TH1* residualHist = new TH1F("residualHist", "residualHist", 50, -5, 5);

  // TODO fix errors
  Float_t chi2 = 0;
  for (Int_t i=1; i<=residual->GetNbinsX(); ++i)
  {
    if (fCurrentESD->GetBinError(i) > 0)
    {
      Float_t value = residual->GetBinContent(i) / fCurrentESD->GetBinError(i);
      if (i > 1)
        chi2 += value * value;
      residual->SetBinContent(i, value);
      residualHist->Fill(value);
    }
    else
    {
      //printf("Residual bin %d set to 0\n", i);
      residual->SetBinContent(i, 0);
    }
    convolutedProj->SetBinError(i, 0);
    residual->SetBinError(i, 0);

    if (i == 200)
      fLastChi2Residuals = chi2;
  }

  residualHist->Fit("gaus", "N");
  delete residualHist;

  printf("Difference (Residuals) is %f for bin 2-200\n", chi2);

  TCanvas* canvas1 = 0;
  if (simple)
  {
    canvas1 = new TCanvas(tmpStr, tmpStr, 900, 400);
    canvas1->Divide(2, 1);
  }
  else
  {
    canvas1 = new TCanvas(tmpStr, tmpStr, 1200, 1200);
    canvas1->Divide(2, 3);
  }

  canvas1->cd(1);
  TH1* proj = (TH1*) mcHist->Clone("proj");
  NormalizeToBinWidth(proj);

  if (normalizeESD)
    NormalizeToBinWidth(fMultiplicityESDCorrected[esdCorrId]);

  proj->GetXaxis()->SetRangeUser(0, 200);
  proj->SetTitle(";true multiplicity;Entries");
  proj->SetStats(kFALSE);
  proj->DrawCopy("HIST");
  gPad->SetLogy();

  //fMultiplicityESDCorrected[esdCorrId]->SetMarkerStyle(3);
  fMultiplicityESDCorrected[esdCorrId]->SetLineColor(2);
  fMultiplicityESDCorrected[esdCorrId]->DrawCopy("SAME HIST E");

  TLegend* legend = new TLegend(0.3, 0.8, 0.93, 0.93);
  legend->AddEntry(proj, "true distribution");
  legend->AddEntry(fMultiplicityESDCorrected[esdCorrId], "unfolded distribution");
  legend->SetFillColor(0);
  legend->Draw();
  // unfortunately does not work. maybe a bug? --> legend->SetTextSizePixels(14);

  canvas1->cd(2);

  gPad->SetLogy();
  fCurrentESD->GetXaxis()->SetRangeUser(0, 200);
  //fCurrentESD->SetLineColor(2);
  fCurrentESD->SetTitle(";measured multiplicity;Entries");
  fCurrentESD->SetStats(kFALSE);
  fCurrentESD->DrawCopy("HIST E");

  convolutedProj->SetLineColor(2);
  //proj2->SetMarkerColor(2);
  //proj2->SetMarkerStyle(5);
  convolutedProj->DrawCopy("HIST SAME");

  legend = new TLegend(0.3, 0.8, 0.93, 0.93);
  legend->AddEntry(fCurrentESD, "measured distribution");
  legend->AddEntry(convolutedProj, "R #otimes unfolded distribution");
  legend->SetFillColor(0);
  legend->Draw();

  TH1* diffMCUnfolded = dynamic_cast<TH1*> (proj->Clone("diffMCUnfolded"));
  diffMCUnfolded->Add(fMultiplicityESDCorrected[esdCorrId], -1);

  /*Float_t chi2 = 0;
  Float_t chi = 0;
  fLastChi2MCLimit = 0;
  Int_t limit = 0;
  for (Int_t i=2; i<=200; ++i)
  {
    if (proj->GetBinContent(i) != 0)
    {
      Float_t value = (proj->GetBinContent(i) - fMultiplicityESDCorrected[esdCorrId]->GetBinContent(i)) / proj->GetBinContent(i);
      chi2 += value * value;
      chi += TMath::Abs(value);

      //printf("%d %f\n", i, chi);

      if (chi2 < 0.2)
        fLastChi2MCLimit = i;

      if (chi < 3)
        limit = i;

    }
  }*/

  chi2 = 0;
  Float_t chi = 0;
  Int_t limit = 0;
  for (Int_t i=1; i<=diffMCUnfolded->GetNbinsX(); ++i)
  {
    if (fMultiplicityESDCorrected[esdCorrId]->GetBinError(i) > 0)
    {
      Double_t value = diffMCUnfolded->GetBinContent(i) / fMultiplicityESDCorrected[esdCorrId]->GetBinError(i);
      if (value > 1e8)
        value = 1e8; //prevent arithmetic exception
      else if (value < -1e8)
        value = -1e8;
      if (i > 1)
      {
        chi2 += value * value;
        chi += TMath::Abs(value);
      }
      diffMCUnfolded->SetBinContent(i, value);
    }
    else
    {
      //printf("diffMCUnfolded bin %d set to 0\n", i);
      diffMCUnfolded->SetBinContent(i, 0);
    }
    if (chi2 < 1000)
      fLastChi2MCLimit = i;
    if (chi < 1000)
      limit = i;
    if (i == 150)
      fLastChi2MC = chi2;
  }

  printf("limits %d %d\n", fLastChi2MCLimit, limit);
  fLastChi2MCLimit = limit;

  printf("Difference (from MC) is %f for bin 2-150. Limit is %d.\n", fLastChi2MC, fLastChi2MCLimit);

  if (!simple)
  {
    canvas1->cd(3);

    diffMCUnfolded->SetTitle("#chi^{2};Npart;(MC - Unfolded) / e");
    //diffMCUnfolded->GetYaxis()->SetRangeUser(-20, 20);
    diffMCUnfolded->GetXaxis()->SetRangeUser(0, 200);
    diffMCUnfolded->DrawCopy("HIST");

    TH1F* fluctuation = new TH1F("fluctuation", "fluctuation", 20, -5, 5);
    for (Int_t i=20; i<=diffMCUnfolded->GetNbinsX(); ++i)
      fluctuation->Fill(diffMCUnfolded->GetBinContent(i));

    new TCanvas;
    fluctuation->Draw();

    /*TLegend* legend = new TLegend(0.6, 0.7, 0.85, 0.85);
    legend->AddEntry(fMultiplicityESDCorrected, "ESD corrected");
    legend->AddEntry(fMultiplicityMC, "MC");
    legend->AddEntry(fMultiplicityESD, "ESD");
    legend->Draw();*/

    canvas1->cd(4);
    //residual->GetYaxis()->SetRangeUser(-0.2, 0.2);
    residual->GetXaxis()->SetRangeUser(0, 200);
    residual->DrawCopy();

    canvas1->cd(5);

    TH1* ratio = (TH1*) fMultiplicityESDCorrected[esdCorrId]->Clone("ratio");
    ratio->Divide(mcHist);
    ratio->SetTitle("Ratio;true multiplicity;Unfolded / MC");
    ratio->GetYaxis()->SetRangeUser(0.5, 1.5);
    ratio->GetXaxis()->SetRangeUser(0, 200);
    ratio->SetStats(kFALSE);
    ratio->Draw("HIST");

    Double_t ratioChi2 = 0;
    fLastChi2MCLimit = 0;
    for (Int_t i=2; i<=150; ++i)
    {
      Float_t value = ratio->GetBinContent(i) - 1;
      if (value > 1e8)
        value = 1e8; //prevent arithmetic exception
      else if (value < -1e8)
        value = -1e8;

      ratioChi2 += value * value;

      if (ratioChi2 < 0.1)
        fLastChi2MCLimit = i;
    }

    printf("Sum over (ratio-1)^2 (2..150) is %f\n", ratioChi2);
    // TODO FAKE
    fLastChi2MC = ratioChi2;
  }

  canvas1->SaveAs(Form("%s.gif", canvas1->GetName()));
}

//____________________________________________________________________
void AliMultiplicityCorrection::GetComparisonResults(Float_t* mc, Int_t* mcLimit, Float_t* residuals)
{
  // Returns the chi2 between the MC and the unfolded ESD as well as between the ESD and the folded unfolded ESD
  // These values are computed during DrawComparison, thus this function picks up the
  // last calculation

  if (mc)
    *mc = fLastChi2MC;
  if (mcLimit)
    *mcLimit = fLastChi2MCLimit;
  if (residuals)
    *residuals = fLastChi2Residuals;
}


//____________________________________________________________________
TH2F* AliMultiplicityCorrection::GetMultiplicityMC(Int_t i, EventType eventType)
{
  //
  // returns the corresponding MC spectrum
  //

  switch (eventType)
  {
    case kTrVtx : return fMultiplicityVtx[i]; break;
    case kMB : return fMultiplicityMB[i]; break;
    case kINEL : return fMultiplicityINEL[i]; break;
  }

  return 0;
}

//____________________________________________________________________
void AliMultiplicityCorrection::ApplyBayesianMethod(Int_t inputRange, Bool_t fullPhaseSpace, EventType eventType, Float_t regPar, Int_t nIterations, TH1* inputDist)
{
  //
  // correct spectrum using bayesian method
  //

  Int_t correlationID = inputRange + ((fullPhaseSpace == kFALSE) ? 0 : 4);

  SetupCurrentHists(inputRange, fullPhaseSpace, eventType, kFALSE);

  // smooth efficiency
  //TH1* tmp = (TH1*) fCurrentEfficiency->Clone("eff_clone");
  //for (Int_t i=2; i<fCurrentEfficiency->GetNbinsX(); ++i)
  //  fCurrentEfficiency->SetBinContent(i, (tmp->GetBinContent(i-1) + tmp->GetBinContent(i) + tmp->GetBinContent(i+1)) / 3);

  // set efficiency to 1 above 150
  // FAKE TEST
  //for (Int_t i=150; i<=fCurrentEfficiency->GetNbinsX(); ++i)
  //  fCurrentEfficiency->SetBinContent(i, 1);

  // normalize correction for given nPart
  for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
  {
    // with this it is normalized to 1
    Double_t sum = fCurrentCorrelation->Integral(i, i, 1, fCurrentCorrelation->GetNbinsY());

    // with this normalized to the given efficiency
    if (fCurrentEfficiency->GetBinContent(i) > 0)
      sum /= fCurrentEfficiency->GetBinContent(i);
    else
      sum = 0;

    for (Int_t j=1; j<=fCurrentCorrelation->GetNbinsY(); ++j)
    {
      if (sum > 0)
      {
        fCurrentCorrelation->SetBinContent(i, j, fCurrentCorrelation->GetBinContent(i, j) / sum);
        fCurrentCorrelation->SetBinError(i, j, fCurrentCorrelation->GetBinError(i, j) / sum);
      }
      else
      {
        fCurrentCorrelation->SetBinContent(i, j, 0);
        fCurrentCorrelation->SetBinError(i, j, 0);
      }
    }
  }

  // normalize nTrack
  /*for (Int_t j=1; j<=fCurrentCorrelation->GetNbinsY(); ++j)
  {
    // with this it is normalized to 1
    Double_t sum = fCurrentCorrelation->Integral(1, fCurrentCorrelation->GetNbinsX(), j, j);

    for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
    {
      if (sum > 0)
      {
        fCurrentCorrelation->SetBinContent(i, j, fCurrentCorrelation->GetBinContent(i, j) / sum);
        fCurrentCorrelation->SetBinError(i, j, fCurrentCorrelation->GetBinError(i, j) / sum);
      }
      else
      {
        fCurrentCorrelation->SetBinContent(i, j, 0);
        fCurrentCorrelation->SetBinError(i, j, 0);
      }
    }
  }*/

  // smooth input spectrum
  /*
  TH1* esdClone = (TH1*) fCurrentESD->Clone("esdClone");

  for (Int_t i=2; i<fCurrentESD->GetNbinsX(); ++i)
    if (esdClone->GetBinContent(i) < 1e-3)
      fCurrentESD->SetBinContent(i, (esdClone->GetBinContent(i-1) + esdClone->GetBinContent(i) + esdClone->GetBinContent(i+1)) / 3);

  delete esdClone;

  // rescale to 1
  fCurrentESD->Scale(1.0 / fCurrentESD->Integral());
  */

  /*new TCanvas;
  fCurrentEfficiency->Draw();

  new TCanvas;
  fCurrentCorrelation->DrawCopy("COLZ");

  new TCanvas;
  ((TH2*) fCurrentCorrelation)->ProjectionX()->DrawCopy();

  new TCanvas;
  ((TH2*) fCurrentCorrelation)->ProjectionY()->DrawCopy();*/

  // pick prior distribution
  TH1* hPrior = 0;
  if (inputDist)
  {
    printf("Using different starting conditions...\n");
    hPrior = (TH1*)inputDist->Clone("prior");
  }
  else
    hPrior = (TH1*)fCurrentESD->Clone("prior");
  Float_t norm = hPrior->Integral();
  for (Int_t t=1; t<=hPrior->GetNbinsX(); t++)
    hPrior->SetBinContent(t, hPrior->GetBinContent(t)/norm);

  // define temp hist
  TH1F* hTemp = (TH1F*)fCurrentESD->Clone("temp");
  hTemp->Reset();

  // just a shortcut
  TH2F* hResponse = (TH2F*) fCurrentCorrelation;

  // histogram to store the inverse response calculated from bayes theorem (from prior and response) IR
  TH2F* hInverseResponseBayes = (TH2F*)hResponse->Clone("pji");
  hInverseResponseBayes->Reset();

  TH1F* convergence = new TH1F("convergence", "convergence", 50, 0.5, 50.5);

  const Int_t kStartBin = 1;

  // unfold...
  for (Int_t i=0; i<nIterations; i++)
  {
    //printf(" iteration %i \n", i);

    // calculate IR from Beyes theorem
    // IR_ji = R_ij * prior_i / sum_k(R_kj * prior_k)

    for (Int_t m=1; m<=hResponse->GetNbinsY(); m++)
    {
      Float_t norm = 0;
      for (Int_t t = kStartBin; t<=hResponse->GetNbinsX(); t++)
        norm += hResponse->GetBinContent(t,m) * hPrior->GetBinContent(t);
      for (Int_t t = kStartBin; t<=hResponse->GetNbinsX(); t++)
      {
        if (norm==0)
          hInverseResponseBayes->SetBinContent(t,m,0);
        else
          hInverseResponseBayes->SetBinContent(t,m, hResponse->GetBinContent(t,m) * hPrior->GetBinContent(t)/norm);
      }
    }

    for (Int_t t = kStartBin; t<=hResponse->GetNbinsX(); t++)
    {
      Float_t value = 0;
      for (Int_t m=1; m<=hResponse->GetNbinsY(); m++)
        value += fCurrentESD->GetBinContent(m) * hInverseResponseBayes->GetBinContent(t,m);

      if (fCurrentEfficiency->GetBinContent(t) > 0)
        hTemp->SetBinContent(t, value / fCurrentEfficiency->GetBinContent(t));
      else
        hTemp->SetBinContent(t, 0);
    }

    // this is the last guess, fill before (!) smoothing
    for (Int_t t=kStartBin; t<=fMultiplicityESDCorrected[correlationID]->GetNbinsX(); t++)
    {
      //as bin error put the difference to the last iteration
      //fMultiplicityESDCorrected[correlationID]->SetBinError(t, hTemp->GetBinContent(t) - fMultiplicityESDCorrected[correlationID]->GetBinContent(t));
      fMultiplicityESDCorrected[correlationID]->SetBinContent(t, hTemp->GetBinContent(t));
      fMultiplicityESDCorrected[correlationID]->SetBinError(t, 0.05 * hTemp->GetBinContent(t));

      //printf(" bin %d content %f \n", t, fMultiplicityESDCorrected[correlationID]->GetBinContent(t));
    }

    /*new TCanvas;
    fMultiplicityESDCorrected[correlationID]->DrawCopy();
    gPad->SetLogy();*/

    // regularization (simple smoothing)
    TH1F* hTrueSmoothed = (TH1F*) hTemp->Clone("truesmoothed");

    for (Int_t t=kStartBin+2; t<hTrueSmoothed->GetNbinsX(); t++)
    {
      Float_t average = (hTemp->GetBinContent(t-1)
                         + hTemp->GetBinContent(t)
                         + hTemp->GetBinContent(t+1)
                         ) / 3.;

      // weight the average with the regularization parameter
      hTrueSmoothed->SetBinContent(t, (1-regPar) * hTemp->GetBinContent(t) + regPar * average);
    }

    // calculate chi2 (change from last iteration)
    Double_t chi2 = 0;

    // use smoothed true (normalized) as new prior
    Float_t norm = 1;
    //for (Int_t t=1; t<=hPrior->GetNbinsX(); t++)
    //  norm = norm + hTrueSmoothed->GetBinContent(t);

    for (Int_t t=kStartBin; t<=hTrueSmoothed->GetNbinsX(); t++)
    {
      Float_t newValue = hTrueSmoothed->GetBinContent(t)/norm;
      Float_t diff = hPrior->GetBinContent(t) - newValue;
      chi2 += (Double_t) diff * diff;

      hPrior->SetBinContent(t, newValue);
    }

    //printf("Chi2 of %d iteration = %.10f\n", i, chi2);

    convergence->Fill(i+1, chi2);

    //if (i % 5 == 0)
    //  DrawComparison(Form("Bayesian_%d", i), mcTarget, correlationID, kTRUE, eventType);

    delete hTrueSmoothed;
  } // end of iterations

  //new TCanvas;
  //convergence->DrawCopy();
  //gPad->SetLogy();

  delete convergence;

  return;

  // ********
  // Calculate the covariance matrix, all arguments are taken from NIM,A362,487-498,1995

  /*printf("Calculating covariance matrix. This may take some time...\n");

  // TODO check if this is the right one...
  TH1* sumHist = GetMultiplicityMC(inputRange, eventType)->ProjectionY("sumHist", 1, GetMultiplicityMC(inputRange, eventType)->GetNbinsX());

  Int_t xBins = hInverseResponseBayes->GetNbinsX();
  Int_t yBins = hInverseResponseBayes->GetNbinsY();

  // calculate "unfolding matrix" Mij
  Float_t matrixM[251][251];
  for (Int_t i=1; i<=xBins; i++)
  {
    for (Int_t j=1; j<=yBins; j++)
    {
      if (fCurrentEfficiency->GetBinContent(i) > 0)
        matrixM[i-1][j-1] = hInverseResponseBayes->GetBinContent(i, j) / fCurrentEfficiency->GetBinContent(i);
      else
        matrixM[i-1][j-1] = 0;
    }
  }

  Float_t* vectorn = new Float_t[yBins];
  for (Int_t j=1; j<=yBins; j++)
    vectorn[j-1] = fCurrentESD->GetBinContent(j);

  // first part of covariance matrix, depends on input distribution n(E)
  Float_t cov1[251][251];

  Float_t nEvents = fCurrentESD->Integral(); // N

  xBins = 20;
  yBins = 20;

  for (Int_t k=0; k<xBins; k++)
  {
    printf("In Cov1: %d\n", k);
    for (Int_t l=0; l<yBins; l++)
    {
      cov1[k][l] = 0;

      // sum_j Mkj Mlj n(Ej) * (1 - n(Ej) / N)
      for (Int_t j=0; j<yBins; j++)
        cov1[k][l] += matrixM[k][j] * matrixM[l][j] * vectorn[j]
          * (1.0 - vectorn[j] / nEvents);

      // - sum_i,j (i != j) Mki Mlj n(Ei) n(Ej) / N
      for (Int_t i=0; i<yBins; i++)
        for (Int_t j=0; j<yBins; j++)
        {
          if (i == j)
            continue;
          cov1[k][l] -= matrixM[k][i] * matrixM[l][j] * vectorn[i]
            * vectorn[j] / nEvents;
         }
    }
  }

  printf("Cov1 finished\n");

  TH2F* cov = (TH2F*) hInverseResponseBayes->Clone("cov");
  cov->Reset();

  for (Int_t i=1; i<=xBins; i++)
    for (Int_t j=1; j<=yBins; j++)
      cov->SetBinContent(i, j, cov1[i-1][j-1]);

  new TCanvas;
  cov->Draw("COLZ");

  // second part of covariance matrix, depends on response matrix
  Float_t cov2[251][251];

  // Cov[P(Er|Cu), P(Es|Cu)] term
  Float_t covTerm[100][100][100];
  for (Int_t r=0; r<yBins; r++)
    for (Int_t u=0; u<xBins; u++)
      for (Int_t s=0; s<yBins; s++)
      {
        if (r == s)
          covTerm[r][u][s] = 1.0 / sumHist->GetBinContent(u+1) * hResponse->GetBinContent(u+1, r+1)
            * (1.0 - hResponse->GetBinContent(u+1, r+1));
        else
          covTerm[r][u][s] = - 1.0 / sumHist->GetBinContent(u+1) * hResponse->GetBinContent(u+1, r+1)
            * hResponse->GetBinContent(u+1, s+1);
      }

  for (Int_t k=0; k<xBins; k++)
    for (Int_t l=0; l<yBins; l++)
    {
      cov2[k][l] = 0;
      printf("In Cov2: %d %d\n", k, l);
      for (Int_t i=0; i<yBins; i++)
        for (Int_t j=0; j<yBins; j++)
        {
          //printf("In Cov2: %d %d %d %d\n", k, l, i, j);
          // calculate Cov(Mki, Mlj) = sum{ru},{su} ...
          Float_t tmpCov = 0;
          for (Int_t r=0; r<yBins; r++)
            for (Int_t u=0; u<xBins; u++)
              for (Int_t s=0; s<yBins; s++)
              {
                if (hResponse->GetBinContent(u+1, r+1) == 0 || hResponse->GetBinContent(u+1, s+1) == 0
                  || hResponse->GetBinContent(u+1, i+1) == 0)
                  continue;

                tmpCov += BayesCovarianceDerivate(matrixM, hResponse, fCurrentEfficiency, k, i, r, u)
                  * BayesCovarianceDerivate(matrixM, hResponse, fCurrentEfficiency, l, j, s, u)
                  * covTerm[r][u][s];
              }

          cov2[k][l] += fCurrentESD->GetBinContent(i+1) * fCurrentESD->GetBinContent(j+1) * tmpCov;
        }
    }

  printf("Cov2 finished\n");

  for (Int_t i=1; i<=xBins; i++)
    for (Int_t j=1; j<=yBins; j++)
      cov->SetBinContent(i, j, cov1[i-1][j-1] + cov2[i-1][j-1]);

  new TCanvas;
  cov->Draw("COLZ");*/
}

//____________________________________________________________________
Float_t AliMultiplicityCorrection::BayesCovarianceDerivate(Float_t matrixM[251][251], TH2* hResponse,
  TH1* fCurrentEfficiency, Int_t k, Int_t i, Int_t r, Int_t u)
{
  //
  // helper function for the covariance matrix of the bayesian method
  //

  Float_t result = 0;

  if (k == u && r == i)
    result += 1.0 / hResponse->GetBinContent(u+1, r+1);

  if (k == u)
    result -= 1.0 / fCurrentEfficiency->GetBinContent(u+1);

  if (r == i)
    result -= matrixM[u][i] * fCurrentEfficiency->GetBinContent(u+1) / hResponse->GetBinContent(u+1, i+1);

  result *= matrixM[k][i];

  return result;
}

//____________________________________________________________________
void AliMultiplicityCorrection::ApplyLaszloMethod(Int_t inputRange, Bool_t fullPhaseSpace, EventType eventType)
{
  //
  // correct spectrum using bayesian method
  //

  Float_t regPar = 0;

  Int_t correlationID = inputRange + ((fullPhaseSpace == kFALSE) ? 0 : 4);
  Int_t mcTarget = ((fullPhaseSpace == kFALSE) ? inputRange : 4);

  SetupCurrentHists(inputRange, fullPhaseSpace, eventType, kFALSE);

  // TODO should be taken from correlation map
  //TH1* sumHist = GetMultiplicityMC(inputRange, eventType)->ProjectionY("sumHist", 1, GetMultiplicityMC(inputRange, eventType)->GetNbinsX());

  // normalize correction for given nPart
  for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
  {
    Double_t sum = fCurrentCorrelation->Integral(i, i, 1, fCurrentCorrelation->GetNbinsY());
    //Double_t sum = sumHist->GetBinContent(i);
    if (sum <= 0)
      continue;
    for (Int_t j=1; j<=fCurrentCorrelation->GetNbinsY(); ++j)
    {
      // npart sum to 1
      fCurrentCorrelation->SetBinContent(i, j, fCurrentCorrelation->GetBinContent(i, j) / sum);
      fCurrentCorrelation->SetBinError(i, j, fCurrentCorrelation->GetBinError(i, j) / sum);
    }
  }

  new TCanvas;
  fCurrentCorrelation->Draw("COLZ");

  // FAKE
  fCurrentEfficiency = ((TH2*) fCurrentCorrelation)->ProjectionX("eff");

  // pick prior distribution
  TH1F* hPrior = (TH1F*)fCurrentESD->Clone("prior");
  Float_t norm = 1; //hPrior->Integral();
  for (Int_t t=1; t<=hPrior->GetNbinsX(); t++)
    hPrior->SetBinContent(t, hPrior->GetBinContent(t)/norm);

  // zero distribution
  TH1F* zero =  (TH1F*)hPrior->Clone("zero");

  // define temp hist
  TH1F* hTemp = (TH1F*)fCurrentESD->Clone("temp");
  hTemp->Reset();

  // just a shortcut
  TH2F* hResponse = (TH2F*) fCurrentCorrelation;

  // unfold...
  Int_t iterations = 25;
  for (Int_t i=0; i<iterations; i++)
  {
    //printf(" iteration %i \n", i);

    for (Int_t m=1; m<=hResponse->GetNbinsY(); m++)
    {
      Float_t value = 0;
      for (Int_t t = 1; t<=hResponse->GetNbinsX(); t++)
        value += hResponse->GetBinContent(t, m) * hPrior->GetBinContent(t);
      hTemp->SetBinContent(m, value);
      //printf("%d %f %f %f\n", m, zero->GetBinContent(m), hPrior->GetBinContent(m), value);
    }

    // regularization (simple smoothing)
    TH1F* hTrueSmoothed = (TH1F*) hTemp->Clone("truesmoothed");

    for (Int_t t=2; t<hTrueSmoothed->GetNbinsX(); t++)
    {
      Float_t average = (hTemp->GetBinContent(t-1) / hTemp->GetBinWidth(t-1)
                         + hTemp->GetBinContent(t) / hTemp->GetBinWidth(t)
                         + hTemp->GetBinContent(t+1) / hTemp->GetBinWidth(t+1)) / 3.;
      average *= hTrueSmoothed->GetBinWidth(t);

      // weight the average with the regularization parameter
      hTrueSmoothed->SetBinContent(t, (1-regPar) * hTemp->GetBinContent(t) + regPar * average);
    }

    for (Int_t m=1; m<=hResponse->GetNbinsY(); m++)
      hTemp->SetBinContent(m, zero->GetBinContent(m) + hPrior->GetBinContent(m) - hTrueSmoothed->GetBinContent(m));

    // fill guess
    for (Int_t t=1; t<=fMultiplicityESDCorrected[correlationID]->GetNbinsX(); t++)
    {
      fMultiplicityESDCorrected[correlationID]->SetBinContent(t, hTemp->GetBinContent(t));
      fMultiplicityESDCorrected[correlationID]->SetBinError(t, 0.05 * hTemp->GetBinContent(t)); // TODO

      //printf(" bin %d content %f \n", t, fMultiplicityESDCorrected[correlationID]->GetBinContent(t));
    }


    // calculate chi2 (change from last iteration)
    Double_t chi2 = 0;

    // use smoothed true (normalized) as new prior
    Float_t norm = 1; //hTrueSmoothed->Integral();

    for (Int_t t=1; t<hTrueSmoothed->GetNbinsX(); t++)
    {
      Float_t newValue = hTemp->GetBinContent(t)/norm;
      Float_t diff = hPrior->GetBinContent(t) - newValue;
      chi2 += (Double_t) diff * diff;

      hPrior->SetBinContent(t, newValue);
    }

    printf("Chi2 of %d iteration = %.10f\n", i, chi2);

    //if (i % 5 == 0)
      DrawComparison(Form("Laszlo_%d", i), inputRange, fullPhaseSpace, kTRUE, GetMultiplicityMC(mcTarget, eventType)->ProjectionY());

    delete hTrueSmoothed;
  } // end of iterations

  DrawComparison("Laszlo", inputRange, fullPhaseSpace, kTRUE, GetMultiplicityMC(mcTarget, eventType)->ProjectionY());
}

//____________________________________________________________________
void AliMultiplicityCorrection::ApplyGaussianMethod(Int_t inputRange, Bool_t fullPhaseSpace)
{
  //
  // correct spectrum using a simple Gaussian approach, that is model-dependent
  //

  Int_t correlationID = inputRange + ((fullPhaseSpace == kFALSE) ? 0 : 4);
  Int_t mcTarget = ((fullPhaseSpace == kFALSE) ? inputRange : 4);

  SetupCurrentHists(inputRange, fullPhaseSpace, kTrVtx, kFALSE);

  NormalizeToBinWidth((TH2*) fCurrentCorrelation);

  TH1D* correction = dynamic_cast<TH1D*> (fCurrentESD->Clone("GaussianMean"));
  correction->SetTitle("GaussianMean");

  TH1D* correctionWidth = dynamic_cast<TH1D*> (fCurrentESD->Clone("GaussianWidth"));
  correction->SetTitle("GaussianWidth");

  for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
  {
    TH1D* proj = (dynamic_cast<TH2*> (fCurrentCorrelation))->ProjectionX("_px", i, i+1);
    proj->Fit("gaus", "0Q");
    correction->SetBinContent(i, proj->GetFunction("gaus")->GetParameter(1));
    correctionWidth->SetBinContent(i, proj->GetFunction("gaus")->GetParameter(2));

    continue;

    // draw for debugging
    new TCanvas;
    proj->DrawCopy();
    proj->GetFunction("gaus")->DrawCopy("SAME");
  }

  TH1* target = fMultiplicityESDCorrected[correlationID];

  Int_t nBins = target->GetNbinsX()*10+1;
  Float_t* binning = new Float_t[nBins];
  for (Int_t i=1; i<=target->GetNbinsX(); ++i)
    for (Int_t j=0; j<10; ++j)
      binning[(i-1)*10 + j] = target->GetXaxis()->GetBinLowEdge(i) + target->GetXaxis()->GetBinWidth(i) / 10 * j;

  binning[nBins-1] = target->GetXaxis()->GetBinUpEdge(target->GetNbinsX());

  TH1F* fineBinned = new TH1F("targetFineBinned", "targetFineBinned", nBins-1, binning);

  for (Int_t i=1; i<=fCurrentCorrelation->GetNbinsX(); ++i)
  {
    Float_t mean = correction->GetBinContent(i);
    Float_t width = correctionWidth->GetBinContent(i);

    Int_t fillBegin = fineBinned->FindBin(mean - width * 5);
    Int_t fillEnd   = fineBinned->FindBin(mean + width * 5);
    //printf("bin %d mean %f width %f, filling from %d to %d\n", i, mean, width, fillBegin, fillEnd);

    for (Int_t j=fillBegin; j <= fillEnd; ++j)
    {
      fineBinned->AddBinContent(j, TMath::Gaus(fineBinned->GetXaxis()->GetBinCenter(j), mean, width, kTRUE) * fCurrentESD->GetBinContent(i));
    }
  }

  for (Int_t i=1; i<=target->GetNbinsX(); ++i)
  {
    Float_t sum = 0;
    for (Int_t j=1; j<=10; ++j)
      sum += fineBinned->GetBinContent((i-1)*10 + j);
    target->SetBinContent(i, sum / 10);
  }

  delete[] binning;

  DrawComparison("Gaussian", inputRange, fullPhaseSpace, kFALSE, GetMultiplicityMC(mcTarget, kTrVtx)->ProjectionY());
}

//____________________________________________________________________
TH2F* AliMultiplicityCorrection::CalculateMultiplicityESD(TH1* inputMC, Int_t correlationMap)
{
  // runs the distribution given in inputMC through the response matrix identified by
  // correlationMap and produces a measured distribution
  // although it is a TH2F the vertex axis is not used at the moment and all entries are filled in mid-vertex
  // if normalized is set, inputMC is expected to be normalized to the bin width

  if (!inputMC)
    return 0;

  if (correlationMap < 0 || correlationMap >= kCorrHists)
    return 0;

  // empty under/overflow bins in x, otherwise Project3D takes them into account
  TH3* hist = fCorrelation[correlationMap];
  for (Int_t y=1; y<=hist->GetYaxis()->GetNbins(); ++y)
  {
    for (Int_t z=1; z<=hist->GetZaxis()->GetNbins(); ++z)
    {
      hist->SetBinContent(0, y, z, 0);
      hist->SetBinContent(hist->GetXaxis()->GetNbins()+1, y, z, 0);
    }
  }

  TH2* corr = (TH2*) hist->Project3D("zy");
  //corr->Rebin2D(2, 1);
  corr->Sumw2();

  // normalize correction for given nPart
  for (Int_t i=1; i<=corr->GetNbinsX(); ++i)
  {
    Double_t sum = corr->Integral(i, i, 1, corr->GetNbinsY());
    if (sum <= 0)
      continue;

    for (Int_t j=1; j<=corr->GetNbinsY(); ++j)
    {
      // npart sum to 1
      corr->SetBinContent(i, j, corr->GetBinContent(i, j) / sum);
      corr->SetBinError(i, j, corr->GetBinError(i, j) / sum);
    }
  }

  TH2F* target = dynamic_cast<TH2F*> (fMultiplicityESD[0]->Clone(Form("%s_measured", inputMC->GetName())));
  target->Reset();

  for (Int_t meas=1; meas<=corr->GetNbinsY(); ++meas)
  {
    Float_t measured = 0;
    Float_t error = 0;

    for (Int_t gen=1; gen<=corr->GetNbinsX(); ++gen)
    {
      Int_t mcGenBin = inputMC->GetXaxis()->FindBin(corr->GetXaxis()->GetBinCenter(gen));

      measured += inputMC->GetBinContent(mcGenBin) * corr->GetBinContent(gen, meas);
      error += inputMC->GetBinError(mcGenBin) * corr->GetBinContent(gen, meas);
    }

    //printf("%f +- %f ; %f +- %f \n", inputMC->GetBinContent(meas), inputMC->GetBinError(meas), measured, error);

    target->SetBinContent(1 + target->GetNbinsX() / 2, meas, measured);
    target->SetBinError(1 + target->GetNbinsX() / 2, meas, error);
  }

  return target;
}

//____________________________________________________________________
void AliMultiplicityCorrection::SetGenMeasFromFunc(TF1* inputMC, Int_t id)
{
  // uses the given function to fill the input MC histogram and generates from that
  // the measured histogram by applying the response matrix
  // this can be used to evaluate if the methods work indepedently of the input
  // distribution
  // WARNING does not respect the vertex distribution, just fills central vertex bin

  if (!inputMC)
    return;

  if (id < 0 || id >= kESDHists)
    return;

  TH2F* mc = fMultiplicityVtx[id];

  mc->Reset();

  for (Int_t i=1; i<=mc->GetNbinsY(); ++i)
  {
    mc->SetBinContent(mc->GetNbinsX() / 2, i, inputMC->Eval(mc->GetYaxis()->GetBinCenter(i)) * mc->GetYaxis()->GetBinWidth(i));
    mc->SetBinError(mc->GetNbinsX() / 2, i, 0);
  }

  //new TCanvas;
  //mc->Draw("COLZ");

  TH1* proj = mc->ProjectionY();

  TString tmp(fMultiplicityESD[id]->GetName());
  delete fMultiplicityESD[id];
  fMultiplicityESD[id] = CalculateMultiplicityESD(proj, id);
  fMultiplicityESD[id]->SetName(tmp);
}
