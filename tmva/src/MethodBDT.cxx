// @(#)root/tmva $Id: MethodBDT.cxx,v 1.42 2006/10/04 22:29:27 andreas.hoecker Exp $ 
// Author: Andreas Hoecker, Joerg Stelzer, Helge Voss, Kai Voss 

/**********************************************************************************
 * Project: TMVA - a Root-integrated toolkit for multivariate data analysis       *
 * Package: TMVA                                                                  *
 * Class  : MethodBDT (BDT = Boosted Decision Trees)                              *
 * Web    : http://tmva.sourceforge.net                                           *
 *                                                                                *
 * Description:                                                                   *
 *      Analysis of Boosted Decision Trees                                        *
 *                                                                                *
 * Authors (alphabetical):                                                        *
 *      Andreas Hoecker <Andreas.Hocker@cern.ch> - CERN, Switzerland              *
 *      Xavier Prudent  <prudent@lapp.in2p3.fr>  - LAPP, France                   *
 *      Helge Voss      <Helge.Voss@cern.ch>     - MPI-KP Heidelberg, Germany     *
 *      Kai Voss        <Kai.Voss@cern.ch>       - U. of Victoria, Canada         *
 *                                                                                *
 * Copyright (c) 2005:                                                            *
 *      CERN, Switzerland,                                                        * 
 *      U. of Victoria, Canada,                                                   * 
 *      MPI-KP Heidelberg, Germany,                                               * 
 *      LAPP, Annecy, France                                                      *
 *                                                                                *
 * Redistribution and use in source and binary forms, with or without             *
 * modification, are permitted according to the terms listed in LICENSE           *
 * (http://tmva.sourceforge.net/LICENSE)                                          *
 **********************************************************************************/

//_______________________________________________________________________
//                                                                      
// Analysis of Boosted Decision Trees                                   
//               
// Boosted decision trees have been successfully used in High Energy 
// Physics analysis for example by the MiniBooNE experiment
// (Yang-Roe-Zhu, physics/0508045). In Boosted Decision Trees, the
// selection is done on a majority vote on the result of several decision
// trees, which are all derived from the same training sample by
// supplying different event weights during the training.
//
// Decision trees: 
//
// successive decision nodes are used to categorize the
// events out of the sample as either signal or background. Each node
// uses only a single discriminating variable to decide if the event is
// signal-like ("goes right") or background-like ("goes left"). This
// forms a tree like structure with "baskets" at the end (leave nodes),
// and an event is classified as either signal or background according to
// whether the basket where it ends up has been classified signal or
// background during the training. Training of a decision tree is the
// process to define the "cut criteria" for each node. The training
// starts with the root node. Here one takes the full training event
// sample and selects the variable and corresponding cut value that gives
// the best separation between signal and background at this stage. Using
// this cut criterion, the sample is then divided into two subsamples, a
// signal-like (right) and a background-like (left) sample. Two new nodes
// are then created for each of the two sub-samples and they are
// constructed using the same mechanism as described for the root
// node. The devision is stopped once a certain node has reached either a
// minimum number of events, or a minimum or maximum signal purity. These
// leave nodes are then called "signal" or "background" if they contain
// more signal respective background events from the training sample.
//
// Boosting: 
//
// the idea behind the boosting is, that signal events from the training
// sample, that end up in a background node (and vice versa) are given a
// larger weight than events that are in the correct leave node. This
// results in a re-weighed training event sample, with which then a new
// decision tree can be developed. The boosting can be applied several
// times (typically 100-500 times) and one ends up with a set of decision
// trees (a forest).
//
// Bagging: 
//
// In this particular variant of the Boosted Decision Trees the boosting
// is not done on the basis of previous training results, but by a simple
// stochasitc re-sampling of the initial training event sample.
//
// Analysis: 
//
// applying an individual decision tree to a test event results in a
// classification of the event as either signal or background. For the
// boosted decision tree selection, an event is successively subjected to
// the whole set of decision trees and depending on how often it is
// classified as signal, a "likelihood" estimator is constructed for the
// event being signal or background. The value of this estimator is the
// one which is then used to select the events from an event sample, and
// the cut value on this estimator defines the efficiency and purity of
// the selection.
//
//_______________________________________________________________________

#include "TMVA/MethodBDT.h"
#include "TMVA/Tools.h"
#include "TMVA/Timer.h"
#include "Riostream.h"
#include "TRandom.h"
#include <algorithm>
#include "TObjString.h"
#include "TMVA/Ranking.h"


using std::vector;

ClassImp(TMVA::MethodBDT)
 
//_______________________________________________________________________
TMVA::MethodBDT::MethodBDT( TString jobName, TString methodTitle, DataSet& theData, 
                            TString theOption, TDirectory* theTargetDir )
   : TMVA::MethodBase( jobName, methodTitle, theData, theOption, theTargetDir )
{
   // the standard constructor for the "boosted decision trees" 
   //
   // MethodBDT (Boosted Decision Trees) options:
   // format and syntax of option string: "nTrees:BoostType:SeparationType:
   //                                      nEventsMin:dummy:
   //                                      nCuts:SignalFraction"
   // nTrees:          number of trees in the forest to be created
   // BoostType:       the boosting type for the trees in the forest (AdaBoost e.t.c..)
   // SeparationType   the separation criterion applied in the node splitting
   // nEventsMin:      the minimum number of events in a node (leaf criteria, stop splitting)
   // nCuts:  the number of steps in the optimisation of the cut for a node
   // SignalFraction:  scale parameter of the number of Bkg events  
   //                  applied to the training sample to simulate different initial purity
   //                  of your data sample. 
   // UseYesNoLeaf     decide if the classification is done simply by the node type, or the S/B
   //                  (from the training) in the leaf node
   // UseWeightedTrees use average classification from the trees, or have the individual trees
   //                  trees in the forest weighted (e.g. log(boostweight) from AdaBoost
   //
   // known SeparationTypes are:
   //    - MisClassificationError
   //    - GiniIndex
   //    - CrossEntropy
   // known BoostTypes are:
   //    - AdaBoost
   //    - Bagging
   InitBDT(); // sets default values

   DeclareOptions();

   ParseOptions();
   
   ProcessOptions();

   // this initialization is only for the training
   if (HasTrainingTree()) {
      if (Verbose())
         cout << "--- " << GetName() << " called " << endl;
      // fill the STL Vector with the event sample 
      this->InitEventSample();
   }
   else{
      cout << "--- " << GetName() << ": Warning: no training Tree given " <<endl;
      cout << "--- " << GetName() << "  you'll not allowed to cal Train e.t.c..."<<endl;
   }

   //book monitoring histograms (currently for AdaBost, only)
   fBoostWeightHist = new TH1F("fBoostWeight","Ada Boost weights",100,1,100);
   fErrFractHist = new TH2F("fErrFractHist","error fraction vs tree number",
                            fNTrees,0,fNTrees,50,0,0.5);
   fMonitorNtuple= new TTree("fMonitorNtuple","BDT variables");
   fMonitorNtuple->Branch("iTree",&fITree,"iTree/I");
   fMonitorNtuple->Branch("boostWeight",&fBoostWeight,"boostWeight/D");
   fMonitorNtuple->Branch("errorFraction",&fErrorFraction,"errorFraction/D");
   fMonitorNtuple->Branch("nNodes",&fNnodes,"nNodes/I");
}

//_______________________________________________________________________
TMVA::MethodBDT::MethodBDT( DataSet& theData, 
                            TString theWeightFile,  
                            TDirectory* theTargetDir )
   : TMVA::MethodBase( theData, theWeightFile, theTargetDir ) 
{
   // constructor for calculating BDT-MVA using previously generatad decision trees
   // the result of the previous training (the decision trees) are read in via the
   // weightfile. Make sure the "theVariables" correspond to the ones used in 
   // creating the "weight"-file
   InitBDT();
  
   DeclareOptions();
}

//_______________________________________________________________________
void TMVA::MethodBDT::DeclareOptions() 
{
   DeclareOptionRef(fNTrees, "NTrees", "number of trees in the forest");
   DeclareOptionRef(fBoostType, "BoostType", "boosting type for the trees in the forest");
   AddPreDefVal(TString("AdaBoost"));
   AddPreDefVal(TString("Bagging"));
   DeclareOptionRef(fUseYesNoLeaf=kTRUE, "UseYesNoLeaf", "use Sig or Bkg node type or the ratio S/B as classification in the leaf node");
   DeclareOptionRef(fUseWeightedTrees=kTRUE, "UseWeightedTrees", "use weighted trees or simple average in classification from the forest");
   DeclareOptionRef(fSepTypeS="GiniIndex", "SeparationType", "separation criterion for node splitting");
   AddPreDefVal(TString("MisClassificationError"));
   AddPreDefVal(TString("GiniIndex"));
   AddPreDefVal(TString("CrossEntropy"));
   AddPreDefVal(TString("SDivSqrtSPlusB"));
   DeclareOptionRef(fNodeMinEvents, "nEventsMin", "minimum number of events in a leaf node");
   DeclareOptionRef(fNCuts, "nCuts", "number of steps during node cut optimisation");
   DeclareOptionRef(fSignalFraction, "SignalFraction", "Bkgd scale parameter to simulate different sample purities");   
   DeclareOptionRef(fPruneStrength, "PruneStrength", "a parameter to adjust the amount of pruning. Should be large enouth such that overtraining is avoided");
}

//_______________________________________________________________________
void TMVA::MethodBDT::ProcessOptions() 
{
   MethodBase::ProcessOptions();

   fSepTypeS.ToLower();
   if      (fSepTypeS == "misclassificationerror") fSepType = new TMVA::MisClassificationError();
   else if (fSepTypeS == "giniindex")              fSepType = new TMVA::GiniIndex();
   else if (fSepTypeS == "crossentropy")           fSepType = new TMVA::CrossEntropy();
   else if (fSepTypeS == "sdivsqrtsplusb")         fSepType = new TMVA::SdivSqrtSplusB();
   else {
      cout << "--- " << this->GetName() << "::ProcessOptions: ERROR Unknown Separation Index option called \n";
      cout << GetOptions() << endl;
      exit(1);
   }     
}

//_______________________________________________________________________
void TMVA::MethodBDT::InitBDT( void )
{
   // common initialisation with defaults for the BDT-Method
   SetMethodName( "BDT" );
   SetMethodType( TMVA::Types::BDT );
   SetTestvarName();

   fNTrees         = 200;
   fBoostType      = "AdaBoost";
   fNodeMinEvents  = 400;
   fNCuts          = 20;
   fSignalFraction =-1.;     // -1 means scaling the signal fraction in the is switched off, any
                             // value > 0 would scale the number of background events in the 
                             // training tree by the corresponding number
   fPruneStrength  = 10;     
}

//_______________________________________________________________________
TMVA::MethodBDT::~MethodBDT( void )
{
   //destructor
   for (UInt_t i=0; i<fEventSample.size(); i++) delete fEventSample[i];
   for (UInt_t i=0; i<fForest.size(); i++) delete fForest[i];
}

//_______________________________________________________________________
void TMVA::MethodBDT::InitEventSample( void )
{
   // write all Events from the Tree into a vector of TMVA::Events, that are 
   // more easily manipulated.  
   // This method should never be called without existing trainingTree, as it
   // the vector of events from the ROOT training tree
   if (!HasTrainingTree()) {
      cout << "--- " << GetName() << ": Error in ::Init(): Data().TrainingTree() is zero pointer"
           << " --> exit(1)" << endl;
      exit(1);
   }

   Int_t nevents = Data().GetNEvtTrain();
   for (int ievt=0; ievt<nevents; ievt++){
      ReadTrainingEvent(ievt);
      fEventSample.push_back(new TMVA::Event(Data().Event()));
      if (fSignalFraction > 0) {
         if (!fEventSample.back()->IsSignal()) 
            fEventSample.back()->SetWeight(fSignalFraction*fEventSample.back()->GetWeight());
      }
   }
}

//_______________________________________________________________________
void TMVA::MethodBDT::Train( void )
{  
   // default sanity checks
   if (!CheckSanity()) { 
      cout << "--- " << GetName() << ": Error: sanity check failed" << endl;
      exit(1);
   }

   cout << "--- " << GetName() << ": I will train "<< fNTrees << " Decision Trees"  
        << " ... patience please" << endl;
   TMVA::Timer timer( fNTrees, GetName() ); 
   Int_t nNodesBeforePruning = 0;
   Int_t nNodesAfterPruning = 0;
   for (int itree=0; itree<fNTrees; itree++){
      timer.DrawProgressBar( itree );

      fForest.push_back(new TMVA::DecisionTree(fSepType,
                                               fNodeMinEvents,fNCuts));
      //use for the training of the next tree only those event, that have together
      //95% of the weight. The others are.. not really important for the tree, but
      //might be large in number ==> ommitting them reduces training ime considerably
      // in order to do this properly, I would have to sort the events according to
      // their weights, but I can get "at least 95%" also by ommitting simply all events
      // for which the weight is less than 0.05
      //
      //       std::vector<Event*> sample;
      //       for (std::vector<Event*>::iterator iev=fEventSample.begin(); 
      // 	   iev != fEventSample.end(); iev++){
      // 	if ((*iev)->GetWeight() > 0.1) sample.push_back(*iev);
      //       }
      //       fNnodes = fForest.back()->BuildTree(sample);
      fForest.back()->SetPruneStrength(fPruneStrength);
      fNnodes = fForest.back()->BuildTree(fEventSample);
      nNodesBeforePruning +=fNnodes;
      fForest.back()->PruneTree();
      fNnodes = fForest.back()->CountNodes();
      nNodesAfterPruning +=fNnodes;
      fBoostWeights.push_back( this->Boost(fEventSample, fForest.back(), itree) );
      fITree = itree;

      fMonitorNtuple->Fill();
   }

   cout << "--- " << GetName() << "Train: average number of nodes before/after pruning : " 
        << nNodesBeforePruning/fNTrees << " / " 
        << nNodesAfterPruning/fNTrees
        << endl;    
   // get elapsed time
   cout << "--- " << GetName() << "Train: elapsed time: " << timer.GetElapsedTime() 
        << "                              "
        << endl;    
}

//_______________________________________________________________________
Double_t TMVA::MethodBDT::Boost( vector<TMVA::Event*> eventSample, TMVA::DecisionTree *dt, Int_t iTree )
{
   // apply the boosting alogrithim (the algorithm is selecte via the the "option" given
   // in the constructor. The return value is the boosting weight 
  
   if      (fBoostType=="AdaBoost") return this->AdaBoost(eventSample, dt);
   else if (fBoostType=="Bagging")  return this->Bagging(eventSample, iTree);
   else {
      cout << "--- " << this->GetName() << "::Boost: ERROR Unknown boost option called \n";
      cout << GetOptions() << endl;
      exit(1);
   }
}

//_______________________________________________________________________
Double_t TMVA::MethodBDT::AdaBoost( vector<TMVA::Event*> eventSample, TMVA::DecisionTree *dt )
{
   // the AdaBoost implementation.
   // a new training sample is generated by weighting 
   // events that are misclassified by the decision tree. The weight
   // applied is w = (1-err)/err or more general:
   //            w = ((1-err)/err)^beta
   // where err is the fracthin of misclassified events in the tree ( <0.5 assuming
   // demanding the that previous selection was better than random guessing)
   // and "beta" beeing a free parameter (standard: beta = 1) that modifies the
   // boosting.

   Double_t adaBoostBeta=1.;   // that's apparently the standard value :)

   Double_t err=0, sumw=0, sumwfalse=0, count=0;
   vector<Bool_t> correctSelected;
   correctSelected.reserve(eventSample.size());
   for (vector<TMVA::Event*>::iterator e=eventSample.begin(); e!=eventSample.end();e++) {
      Bool_t isSignalType= (dt->CheckEvent(*(*e),fUseYesNoLeaf) > 0.5 ) ? 1 : 0;
      sumw+=(*e)->GetWeight();

      if (isSignalType == (*e)->IsSignal()) { 
         correctSelected.push_back(kTRUE);
      }
      else{
         sumwfalse+= (*e)->GetWeight();
         count+=1;
         correctSelected.push_back(kFALSE);
      }    
   }
   err=sumwfalse/sumw;

   Double_t newSumw=0;
   Int_t i=0;
   Double_t boostWeight;
   if (err>0){
      if (adaBoostBeta == 1){
         boostWeight = (1-err)/err ;
      }
      else {
         boostWeight =  pow((1-err)/err,adaBoostBeta) ;
      }
   }
   else {
      boostWeight = 1000; // 
   }

   for (vector<TMVA::Event*>::iterator e=eventSample.begin(); e!=eventSample.end();e++){
      if (!correctSelected[i]){
         (*e)->SetWeight( (*e)->GetWeight() * boostWeight);
      }
      newSumw+=(*e)->GetWeight();    
      i++;
   }

   // re-normalise the Weights
   for (vector<TMVA::Event*>::iterator e=eventSample.begin(); e!=eventSample.end();e++){
      (*e)->SetWeight( (*e)->GetWeight() * sumw / newSumw );
   }

   fBoostWeightHist->Fill(boostWeight);
   fErrFractHist->Fill(fForest.size(),err);

   fBoostWeight = boostWeight;
   fErrorFraction = err;
  
   return log(boostWeight);
}

//_______________________________________________________________________
Double_t TMVA::MethodBDT::Bagging( vector<TMVA::Event*> eventSample, Int_t iTree )
{
   // call it Bootstrapping, re-sampling or whatever you like, in the end it is nothing
   // else but applying "random Weights" to each event.
   Double_t newSumw=0;
   Double_t newWeight;
   TRandom *trandom   = new TRandom(iTree);
   for (vector<TMVA::Event*>::iterator e=eventSample.begin(); e!=eventSample.end();e++) {
      newWeight = trandom->Rndm();
      (*e)->SetWeight(newWeight);
      newSumw+=(*e)->GetWeight();    
   }
   for (vector<TMVA::Event*>::iterator e=eventSample.begin(); e!=eventSample.end();e++) {
      (*e)->SetWeight( (*e)->GetWeight() * eventSample.size() / newSumw );
   }
   return 1.;  //here as there are random weights for each event, just return a constant==1;
}

//_______________________________________________________________________
void TMVA::MethodBDT::WriteWeightsToStream( ostream& o) const
{  
   // and save the Weights
   o << "NTrees= " << fForest.size() <<endl; 
   for (UInt_t i=0; i< fForest.size(); i++){
      o << "-999 *******Tree " << i << "  boostWeight " << fBoostWeights[i] << endl;
      (fForest[i])->Print(o);
   }
}
  
//_______________________________________________________________________
void  TMVA::MethodBDT::ReadWeightsFromStream( istream& istr )
{
   // read variable names and min/max
   // NOTE: the latter values are mandatory for the normalisation 
   // in the reader application !!!
   TString var, dummy;

   // and read the Weights (BDT coefficients)  
   istr >> dummy >> fNTrees;
   cout << "--- " << GetName() << ": Read " << fNTrees << " Decision trees\n";
  
   for (UInt_t i=0;i<fForest.size();i++) delete fForest[i];
   fForest.clear();
   fBoostWeights.clear();
   Int_t iTree;
   Double_t boostWeight;
   istr >> var >> var;
   for (int i=0;i<fNTrees;i++){
      istr >> iTree >> dummy >> boostWeight;
      if (iTree != i) {
         cout << "--- "  << ": Error while reading Weight file \n ";
         cout << "--- "  << ": mismatch Itree="<<iTree<<" i="<<i<<endl;
         exit(1);
      }
      TMVA::DecisionTreeNode *n = new TMVA::DecisionTreeNode();
      TMVA::NodeID id;
      n->ReadRec(istr, id, Data().Event());
      fForest.push_back(new TMVA::DecisionTree());
      fForest.back()->SetRoot(n);
      fBoostWeights.push_back(boostWeight);
   }
}

//_______________________________________________________________________
Double_t TMVA::MethodBDT::GetMvaValue()
{
   // return the MVA value (range [-1;1]) that classifies the
   // event.according to the majority vote from the total number of
   // decision trees
   // In the literature I found that people actually use the 
   // weighted majority vote (using the boost weights) .. However I
   // did not see any improvement in doing so :(  
   // --> this is currently switched off

   Double_t myMVA = 0;
   Double_t norm  = 0;
   for (UInt_t itree=0; itree<fForest.size(); itree++){
      //
      if (fUseWeightedTrees){ 
         myMVA += fBoostWeights[itree] * fForest[itree]->CheckEvent(Data().Event(),fUseYesNoLeaf);
         norm  += fBoostWeights[itree];
      }
      else { 
         myMVA += fForest[itree]->CheckEvent(Data().Event(),fUseYesNoLeaf);
         norm  += 1;
      }
   }
   return myMVA /= Double_t(norm);
}

//_______________________________________________________________________
void  TMVA::MethodBDT::WriteHistosToFile( void ) const
{
   // here we could write some histograms created during the processing
   // to the output file.
   cout << "--- " << GetName() << ": write"
        <<" monitoring histograms to file: " << BaseDir()->GetPath() << endl;

   BaseDir()->cd();
   fBoostWeightHist->Write();
   fErrFractHist->Write();
   fMonitorNtuple->Write();
}

// return the individual relative variable importance 
vector< Double_t > TMVA::MethodBDT::GetVariableImportance()
{
   // return the relative variable importance, normalized to all
   // variables together having the importance 1. The importance in
   // evaluated as the total separation-gain that this variable had in
   // the decision trees (weighted by the number of events)
  
   fVariableImportance.resize(GetNvar());
   Double_t  sum=0;
   for (int itree = 0; itree < fNTrees; itree++){
      vector<Double_t> relativeImportance(fForest[itree]->GetVariableImportance());
      for (unsigned int i=0; i< relativeImportance.size(); i++) {
         fVariableImportance[i] += relativeImportance[i] ;
      } 
   }   
   for (unsigned int i=0; i< fVariableImportance.size(); i++) sum += fVariableImportance[i];
   for (unsigned int i=0; i< fVariableImportance.size(); i++) fVariableImportance[i] /= sum;

   return fVariableImportance;
}

Double_t TMVA::MethodBDT::GetVariableImportance(UInt_t ivar)
{
   vector<Double_t> relativeImportance = this->GetVariableImportance();
   if (ivar < (UInt_t)relativeImportance.size()) return relativeImportance[ivar];
   else {
      cout << "--- TMVA::MethodBDT::GetVariableImportance(ivar)  ERROR!!" <<endl;
      cout << "---                  ivar = " << ivar << " is out of range " <<endl;
      exit(1);
   }
}

const TMVA::Ranking* TMVA::MethodBDT::CreateRanking()
{
   // computes ranking of input variables

   // create the ranking object
   fRanking = new Ranking( GetName(), "Variable Importance" );
   vector< Double_t> importance(this->GetVariableImportance());

   for (Int_t ivar=0; ivar<GetNvar(); ivar++) {
      fRanking->AddRank( *new Rank( GetInputExp(ivar), importance[ivar] ) );
   }

   return fRanking;
}
