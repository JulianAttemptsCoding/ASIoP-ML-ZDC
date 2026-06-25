// zdc_make_graphs_v5.C — regression-focused ZDC graph pipeline
// Goal: rebuild only the energy-reconstruction graphs from raw outfile_*.root files.
// Gold-standard references:
//   - gamma main window:   [1,40] GeV; MeV files are kept in QA but excluded from main regression graphs.
//   - neutron main window: [20,300] GeV; any lower-energy neutron file is kept and can be included in training diagnostics.
// Regression fixes vs v4:
//   1) Restore the original working macro's quadratic regression basis:
//        Erec = p0 + p1*ECAL + p2*HCAL + p3*ECAL^2 + p4*HCAL^2
//      The slide text's compact quadratic formula was not the behavior of the older working macro.
//   2) Add p0 modes so p0 is not silently forced large:
//        p0free  = ordinary intercept, closest to the older ROOT macro / PDF behavior.
//        p0zero  = p0 fixed exactly 0.
//        p0ridge = p0 softly constrained near 0 with sigma = 0.25 GeV.
//   3) Use scale-only feature normalization. No centering is used, so p0 remains a real physical intercept.
//   4) Keep all raw data in QA CSVs and TBrowser; main plots still use the PDF energy windows.
//
// Run:
//   root -l -q 'scripts/zdc_make_graphs_v5.C("data","plots","qa")'

#include <TFile.h>
#include <TTree.h>
#include <TLeaf.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TH1D.h>
#include <TF1.h>
#include <TLegend.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TLine.h>
#include <TMath.h>
#include <TMatrixD.h>
#include <TVectorD.h>
#include <TDecompSVD.h>
#include <TString.h>
#include <TDirectory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

using std::string;
using std::vector;

static const double GAMMA_MAIN_MIN_GEV   = 0.999;
static const double GAMMA_MAIN_MAX_GEV   = 40.5;
static const double NEUTRON_EVAL_MIN_GEV = 19.5;
static const double NEUTRON_EVAL_MAX_GEV = 300.5;
static const double VIS_CUT_FRAC         = 0.001;
static const double P0_RIDGE_SIGMA_GEV   = 0.25;

static const char* METHOD_LABEL[3] = {"Linear Fun.", "Ratio Fun.", "Quadratic Fun."};
static const char* WEIGHT_LABEL[2] = {"Equal-Weighted", "#sqrt{E_{Beam}}-weighted"};

struct EventData { double ecal, hcal, beam; };
struct Sample {
    string particle, path, basename;
    double beamMean, beamRMS;
    vector<EventData> events;
};
struct Model {
    int method, weight, p0mode;
    bool ok;
    int nUsed;
    vector<double> scale;
    vector<double> beta;    // if p0mode==1: no intercept; else beta[0]=true p0, beta[j+1] maps feature[j]/scale[j]
};
struct Metric {
    double E, mu, sig, muErr, sigErr;
    int nEvents;
    bool fitOk;
    TH1D* hist;
};
struct CurveDef { int method, weight, color, marker; };

CurveDef curves[6] = {
    {0,0,kRed+1,   kFullCircle},
    {1,0,kRed+1,   kOpenCircle},
    {2,0,kRed+1,   kFullSquare},
    {0,1,kGreen+2, kFullCircle},
    {1,1,kGreen+2, kOpenCircle},
    {2,1,kGreen+2, kFullSquare}
};

string lowerCopy(string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); });
    return s;
}
string baseName(const string& p) {
    size_t pos = p.find_last_of("/\\");
    return (pos == string::npos) ? p : p.substr(pos+1);
}
bool endsWith(const string& s, const string& suf) {
    return s.size() >= suf.size() && s.compare(s.size()-suf.size(), suf.size(), suf) == 0;
}
void ensureDir(const char* p) { if (gSystem->AccessPathName(p)) gSystem->mkdir(p, true); }

string duplicateStem(string filename) {
    string s = lowerCopy(filename);
    size_t dot = s.rfind(".root");
    if (dot != string::npos) s = s.substr(0,dot);
    size_t p = s.rfind('('), q = s.rfind(')');
    if (p != string::npos && q == s.size()-1 && p < q) {
        bool digits = true;
        for (size_t i=p+1;i<q;i++) if (!std::isdigit((unsigned char)s[i])) digits=false;
        if (digits) s = s.substr(0,p);
    }
    return s;
}

void setStyle() {
    gStyle->SetOptStat(0);
    gStyle->SetOptFit(0);
    gStyle->SetCanvasColor(kWhite);
    gStyle->SetPadColor(kWhite);
    gStyle->SetFrameFillColor(kWhite);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);
    gStyle->SetGridStyle(3);
    gStyle->SetGridWidth(1);
    gStyle->SetTitleFont(42,"XYZ");
    gStyle->SetLabelFont(42,"XYZ");
    gStyle->SetLegendFont(42);
}

TLeaf* getLeafAny(TTree* t, const vector<const char*>& names) {
    for (auto n : names) { TLeaf* l=t->GetLeaf(n); if (l) return l; }
    return nullptr;
}

void collectFiles(const string& dir, const string& particle, vector<string>& out) {
    TSystemDirectory sd(dir.c_str(), dir.c_str());
    TList* files = sd.GetListOfFiles();
    if (!files) return;
    TIter next(files);
    TObject* obj=nullptr;
    while ((obj=next())) {
        TSystemFile* f=(TSystemFile*)obj;
        string name=f->GetName();
        if (name=="." || name=="..") continue;
        string full=dir+"/"+name;
        if (f->IsDirectory()) { collectFiles(full, particle, out); continue; }
        string low=lowerCopy(name);
        if (endsWith(low,".root") && low.find("outfile")!=string::npos && low.find(particle)!=string::npos) out.push_back(full);
    }
    delete files;
}

bool loadSample(const string& path, const string& particle, Sample& s) {
    TFile* f = TFile::Open(path.c_str(), "READ");
    if (!f || f->IsZombie()) { printf("[ERROR] cannot open %s\n", path.c_str()); return false; }
    TTree* t = (TTree*)f->Get("events");
    if (!t) { printf("[ERROR] no events tree in %s\n", path.c_str()); f->Close(); return false; }

    TLeaf* lEcal = getLeafAny(t,{"EcalFarForwardZDCHits.energy","EcalFarForwardZDCHits/EcalFarForwardZDCHits.energy"});
    TLeaf* lHcal = getLeafAny(t,{"HcalFarForwardZDCHits.energy","HcalFarForwardZDCHits/HcalFarForwardZDCHits.energy"});
    TLeaf* lPx   = getLeafAny(t,{"MCParticles.momentum.x","MCParticles/MCParticles.momentum.x"});
    TLeaf* lPy   = getLeafAny(t,{"MCParticles.momentum.y","MCParticles/MCParticles.momentum.y"});
    TLeaf* lPz   = getLeafAny(t,{"MCParticles.momentum.z","MCParticles/MCParticles.momentum.z"});
    TLeaf* lMass = getLeafAny(t,{"MCParticles.mass","MCParticles/MCParticles.mass"});
    TLeaf* lStat = getLeafAny(t,{"MCParticles.generatorStatus","MCParticles/MCParticles.generatorStatus"});
    if (!lEcal || !lHcal || !lPx || !lPy || !lPz || !lMass) {
        printf("[ERROR] missing required leaves in %s\n", path.c_str()); f->Close(); return false;
    }

    s.particle=particle; s.path=path; s.basename=baseName(path); s.events.clear();
    double sum=0, sum2=0; long long n=0;
    for (Long64_t iev=0; iev<t->GetEntries(); ++iev) {
        t->GetEntry(iev);
        double ecal=0, hcal=0;
        for (int i=0;i<lEcal->GetLen();++i) ecal += lEcal->GetValue(i);
        for (int i=0;i<lHcal->GetLen();++i) hcal += lHcal->GetValue(i);
        int ip=0;
        if (lStat) for (int k=0;k<lStat->GetLen();++k) if ((int)lStat->GetValue(k)==1) { ip=k; break; }
        double px=lPx->GetValue(ip), py=lPy->GetValue(ip), pz=lPz->GetValue(ip), m=lMass->GetValue(ip);
        double beam = std::sqrt(px*px+py*py+pz*pz+m*m);
        if (!std::isfinite(beam) || beam<=0) continue;
        s.events.push_back({ecal,hcal,beam}); sum+=beam; sum2+=beam*beam; n++;
    }
    f->Close();
    if (n==0) return false;
    s.beamMean=sum/n;
    s.beamRMS=std::sqrt(TMath::Max(0.0,sum2/n-s.beamMean*s.beamMean));
    printf("[LOAD] %-7s %-35s events=%5lld E=%.6g GeV\n", particle.c_str(), s.basename.c_str(), n, s.beamMean);
    return true;
}

void mergeSameEnergy(vector<Sample>& v) {
    std::sort(v.begin(), v.end(), [](const Sample&a,const Sample&b){return a.beamMean<b.beamMean;});
    vector<Sample> out;
    for (const auto& s : v) {
        if (out.empty()) { out.push_back(s); continue; }
        Sample& last=out.back();
        double tol=TMath::Max(1e-5,1e-4*TMath::Max(1.0,last.beamMean));
        if (std::fabs(last.beamMean-s.beamMean)<=tol) {
            printf("[MERGE] same E %.6g: %s + %s\n", last.beamMean, last.basename.c_str(), s.basename.c_str());
            last.events.insert(last.events.end(), s.events.begin(), s.events.end());
            last.basename += "+" + s.basename;
            double sum=0,sum2=0; for (const auto&e:last.events) {sum+=e.beam; sum2+=e.beam*e.beam;}
            double n=last.events.size(); last.beamMean=sum/n; last.beamRMS=std::sqrt(TMath::Max(0.0,sum2/n-last.beamMean*last.beamMean));
        } else out.push_back(s);
    }
    v.swap(out);
}

void loadAll(const char* inputDir, const string& particle, vector<Sample>& samples) {
    vector<string> files; collectFiles(inputDir, particle, files); std::sort(files.begin(), files.end());
    std::set<string> seen;
    for (auto p : files) {
        string stem=duplicateStem(baseName(p));
        if (seen.count(stem)) { printf("[SKIP duplicate filename copy] %s\n", p.c_str()); continue; }
        seen.insert(stem);
        Sample s; if (loadSample(p, particle, s)) samples.push_back(s);
    }
    mergeSameEnergy(samples);
    std::sort(samples.begin(), samples.end(), [](const Sample&a,const Sample&b){return a.beamMean<b.beamMean;});
}

vector<Sample> filterE(const vector<Sample>& in, double lo, double hi) {
    vector<Sample> out; for (auto&s:in) if (s.beamMean>=lo && s.beamMean<=hi) out.push_back(s); return out;
}

vector<double> features(const EventData& e, int method) {
    double tot=e.ecal+e.hcal;
    if (method==0) return {e.ecal,e.hcal};
    if (method==1) return {e.ecal,e.hcal,(tot>1e-12?e.hcal/tot:0.0)};
    // Restored original working macro's m2 basis.
    return {e.ecal,e.hcal,e.ecal*e.ecal,e.hcal*e.hcal};
}

const char* p0Name(int mode) {
    if (mode==0) return "p0free";
    if (mode==1) return "p0zero";
    return "p0ridge";
}

Model fitModel(const vector<Sample>& train, int method, int weight, int p0mode,
               const string& tag, std::ofstream& csv) {
    Model m; m.method=method; m.weight=weight; m.p0mode=p0mode; m.ok=false; m.nUsed=0;
    int nf = (method==0?2:(method==1?3:4));
    m.scale.assign(nf,1.0);
    int k = nf + (p0mode==1 ? 0 : 1);
    m.beta.assign(k,0.0);

    // Scale-only normalization: p0 remains a real intercept, not a hidden centered offset.
    vector<double> maxabs(nf,0.0);
    for (const auto&s:train) for (const auto&e:s.events) {
        double vis=e.ecal+e.hcal; if (e.beam<=0 || vis<VIS_CUT_FRAC*e.beam) continue;
        vector<double> x=features(e,method);
        for (int j=0;j<nf;++j) maxabs[j]=TMath::Max(maxabs[j], std::fabs(x[j]));
    }
    for (int j=0;j<nf;++j) if (maxabs[j]>1e-12) m.scale[j]=maxabs[j];

    TMatrixD A(k,k); TVectorD b(k);
    auto addRow = [&](const vector<double>& row, double y, double w) {
        for (int i=0;i<k;++i) {
            b(i) += w*row[i]*y;
            for (int j=0;j<k;++j) A(i,j) += w*row[i]*row[j];
        }
    };

    for (const auto&s:train) for (const auto&e:s.events) {
        double vis=e.ecal+e.hcal; if (!std::isfinite(vis) || e.beam<=0 || vis<VIS_CUT_FRAC*e.beam) continue;
        vector<double> x=features(e,method);
        vector<double> row(k,0.0); int off=0;
        if (p0mode!=1) { row[0]=1.0; off=1; }
        for (int j=0;j<nf;++j) row[off+j]=x[j]/m.scale[j];
        double w=(weight==1)?std::sqrt(e.beam):1.0;
        addRow(row,e.beam,w);
        m.nUsed++;
    }
    if (p0mode==2) {
        // p0 ~ 0 constraint.  Equivalent to adding one pseudo-measurement p0=0.
        // Weight = 1/sigma^2; with sigma=0.25 GeV it strongly discourages multi-GeV p0.
        vector<double> row(k,0.0); row[0]=1.0;
        addRow(row,0.0,1.0/(P0_RIDGE_SIGMA_GEV*P0_RIDGE_SIGMA_GEV));
    }
    if (m.nUsed < k+10) { printf("[WARN] not enough rows %s m%d w%d %s\n", tag.c_str(), method, weight, p0Name(p0mode)); return m; }
    TDecompSVD svd(A); Bool_t ok=kTRUE; TVectorD sol=svd.Solve(b,ok);
    if (!ok) { printf("[WARN] SVD failed %s m%d w%d %s\n", tag.c_str(), method, weight, p0Name(p0mode)); return m; }
    for (int i=0;i<k;++i) m.beta[i]=sol(i);
    m.ok=true;

    double p0 = (p0mode==1)?0.0:m.beta[0];
    printf("[FIT] %-16s %-7s m%d w%d n=%d p0=%.6g", tag.c_str(), p0Name(p0mode), method, weight, m.nUsed, p0);
    for (double x:m.beta) printf(" %.6g",x); printf("\n");

    csv << tag << "," << p0Name(p0mode) << "," << method << "," << weight << "," << m.nUsed << "," << p0;
    for (int j=0;j<nf;++j) {
        int off=(p0mode==1?0:1);
        double raw = m.beta[off+j]/m.scale[j];
        csv << "," << raw;
    }
    csv << "\n";
    return m;
}

double predict(const Model& m, const EventData& e) {
    if (!m.ok) return 0.0;
    vector<double> x=features(e,m.method);
    double y=0.0; int off=0;
    if (m.p0mode!=1) { y=m.beta[0]; off=1; }
    for (int j=0;j<(int)x.size();++j) y += m.beta[off+j]*(x[j]/m.scale[j]);
    return y;
}

Double_t dcbFunc(Double_t *x, Double_t *p) {
    double sig=TMath::Abs(p[1]); if (sig<=1e-12) return 0.0;
    double t=(x[0]-p[0])/sig;
    double aL=TMath::Max(1e-4,TMath::Abs(p[2])), nL=TMath::Max(1.01,p[3]);
    double aR=TMath::Max(1e-4,TMath::Abs(p[4])), nR=TMath::Max(1.01,p[5]);
    double val=0.0;
    if (t<-aL) { double A=TMath::Power(nL/aL,nL)*TMath::Exp(-0.5*aL*aL); double B=nL/aL-aL; double arg=B-t; val=(arg>0?A*TMath::Power(arg,-nL):0); }
    else if (t>aR) { double A=TMath::Power(nR/aR,nR)*TMath::Exp(-0.5*aR*aR); double B=nR/aR-aR; double arg=B+t; val=(arg>0?A*TMath::Power(arg,-nR):0); }
    else val=TMath::Exp(-0.5*t*t);
    return p[6]*val;
}

bool coreStats(const vector<double>& vals0, double& mu, double& sig, double& muE, double& sigE) {
    vector<double> vals; for(double x:vals0) if(std::isfinite(x)) vals.push_back(x);
    if(vals.size()<8) {mu=sig=muE=sigE=0; return false;}
    std::sort(vals.begin(), vals.end());
    auto q=[&](double p){ double pos=p*(vals.size()-1); int i0=floor(pos), i1=ceil(pos); if(i0==i1) return vals[i0]; double t=pos-i0; return vals[i0]*(1-t)+vals[i1]*t; };
    double med=q(0.5), core=0.5*(q(0.84)-q(0.16)); if(core<1e-6) core=0.1;
    double lo=med-2.5*core, hi=med+2.5*core, s=0,s2=0,n=0;
    for(double x:vals) if(x>=lo&&x<=hi){s+=x;s2+=x*x;n++;}
    if(n<8){s=0;s2=0;n=0; for(double x:vals){s+=x;s2+=x*x;n++;}}
    mu=s/n; sig=std::sqrt(TMath::Max(0.0,s2/n-mu*mu)); muE=sig/std::sqrt(n); sigE=(n>2?sig/std::sqrt(2*(n-1)):0);
    return sig>0 && std::isfinite(sig);
}

bool fitDCB(TH1D* h, const vector<double>& ratios, double& mu, double& sig, double& muE, double& sigE) {
    mu=sig=muE=sigE=0; if(!h || h->GetEntries()<20) return coreStats(ratios,mu,sig,muE,sigE);
    double xmin=h->GetXaxis()->GetXmin(), xmax=h->GetXaxis()->GetXmax();
    int modeBin=h->GetMaximumBin(); double hmode=h->GetXaxis()->GetBinCenter(modeBin);
    double sw=0,swd=0,swd2=0;
    for(int b=1;b<=h->GetNbinsX();++b){ double x=h->GetXaxis()->GetBinCenter(b), c=h->GetBinContent(b); if(c>0 && TMath::Abs(x-hmode)<=0.8){double d=x-hmode; sw+=c;swd+=c*d;swd2+=c*d*d;} }
    if(sw<5) return coreStats(ratios,mu,sig,muE,sigE);
    double loc=hmode+swd/sw; double var=swd2/sw-(swd/sw)*(swd/sw); double rms=(var>1e-8?std::sqrt(var):h->GetRMS()*0.5); if(rms<0.01) rms=TMath::Max(0.02,h->GetRMS()*0.5);
    double fitLo=TMath::Max(xmin,loc-3.5*rms), fitHi=TMath::Min(xmax,loc+3.5*rms); if(fitHi-fitLo<0.05){fitLo=TMath::Max(xmin,loc-0.3);fitHi=TMath::Min(xmax,loc+0.3);} 
    TF1* f=new TF1(Form("dcb_tmp_%s",h->GetName()),dcbFunc,fitLo,fitHi,7);
    double hRMS=h->GetRMS(); if(hRMS<0.01) hRMS=rms;
    f->SetParameters(loc,TMath::Max(rms,0.7*hRMS),1.2,5.0,1.5,5.0,h->GetMaximum()*rms*2.5);
    f->SetParLimits(0,loc-rms,loc+rms);
    f->SetParLimits(1,TMath::Max(0.005,0.5*hRMS),TMath::Min(2.5,2.0*hRMS));
    f->SetParLimits(2,0.3,8.0); f->SetParLimits(3,1.1,50.0); f->SetParLimits(4,0.3,8.0); f->SetParLimits(5,1.1,50.0); f->SetParLimits(6,0,1e12);
    int status=h->Fit(f,"QNR");
    mu=f->GetParameter(0); sig=TMath::Abs(f->GetParameter(1)); muE=f->GetParError(0); sigE=f->GetParError(1);
    bool sane=(status==0 && std::isfinite(mu)&&std::isfinite(sig)&&sig>0&&mu>xmin&&mu<xmax&&TMath::Abs(mu-loc)<=1.5*rms);
    delete f;
    if(!sane) return coreStats(ratios,mu,sig,muE,sigE);
    if(muE<=0 || muE>0.2) muE=sig/std::sqrt(TMath::Max(1.0,h->GetEntries()));
    if(sigE<=0 || sigE>0.2) sigE=sig/std::sqrt(2*TMath::Max(1.0,h->GetEntries()-1));
    return true;
}

vector<Metric> metricsFor(const vector<Sample>& eval, const Model& model, const string& tag, const string& particle, std::ofstream& csv) {
    vector<Metric> out;
    bool gamma=(particle=="gamma"); int nbins=gamma?100:140; double xmax=gamma?2.0:5.0;
    for(const auto&s:eval){
        TH1D* h=new TH1D(Form("h_%s_%s_m%d_w%d_E%.3f",tag.c_str(),p0Name(model.p0mode),model.method,model.weight,s.beamMean),";E_{rec}/E_{beam};Count",nbins,0,xmax);
        h->SetDirectory(nullptr); h->SetLineColor(kBlue+2); h->SetLineWidth(2);
        vector<double> ratios;
        for(const auto&e:s.events){ double vis=e.ecal+e.hcal; if(e.beam<=0||vis<VIS_CUT_FRAC*e.beam) continue; double r=predict(model,e)/e.beam; if(!std::isfinite(r)) continue; ratios.push_back(r); if(r>=0&&r<=xmax) h->Fill(r); }
        double mu,sig,muE,sigE; bool ok=fitDCB(h,ratios,mu,sig,muE,sigE);
        csv << tag << "," << particle << "," << p0Name(model.p0mode) << "," << model.method << "," << model.weight << "," << s.beamMean << "," << ratios.size() << "," << mu << "," << sig << "," << muE << "," << sigE << "," << (ok?1:0) << "\n";
        printf("[POINT] %-14s %-7s %-7s m%d w%d E=%7.2f mu=%7.3f sig=%7.3f ok=%d\n",tag.c_str(),particle.c_str(),p0Name(model.p0mode),model.method,model.weight,s.beamMean,mu,sig,ok?1:0);
        out.push_back({s.beamMean,mu,sig,muE,sigE,(int)ratios.size(),ok,h});
    }
    std::sort(out.begin(),out.end(),[](const Metric&a,const Metric&b){return a.E<b.E;});
    return out;
}

TGraphErrors* graphFrom(const vector<Metric>& ms, bool reso, const char* name) {
    TGraphErrors* g=new TGraphErrors(ms.size()); g->SetName(name);
    for(int i=0;i<(int)ms.size();++i){ g->SetPoint(i,ms[i].E,reso?ms[i].sig:ms[i].mu); double ey=reso?ms[i].sigErr:ms[i].muErr; if(!std::isfinite(ey)||ey>0.15) ey=0; g->SetPointError(i,0,ey); }
    return g;
}
void styleGraph(TGraphErrors* g,int color,int marker){g->SetMarkerColor(color);g->SetLineColor(color);g->SetMarkerStyle(marker);g->SetMarkerSize(1.1);g->SetLineWidth(2);} 
void stylePad(){gPad->SetLeftMargin(0.13);gPad->SetBottomMargin(0.13);gPad->SetRightMargin(0.04);gPad->SetTopMargin(0.09);gPad->SetTicks(1,1);gPad->SetGrid(1,1);} 
void drawReq(const string& particle,double xmin,double xmax){ bool gamma=(particle=="gamma"); double oldA=gamma?0.35:0.50, newA=gamma?0.20:0.35; TF1* old=new TF1(Form("req_old_%s",particle.c_str()),Form("%g/sqrt(x)+0.05",oldA),TMath::Max(1.0,xmin),xmax); TF1* now=new TF1(Form("req_now_%s",particle.c_str()),Form("%g/sqrt(x)+0.05",newA),TMath::Max(1.0,xmin),xmax); old->SetLineColor(kBlack); old->SetLineWidth(2); now->SetLineColor(kBlue); now->SetLineWidth(2); old->Draw("SAME"); now->Draw("SAME"); TLatex lx; lx.SetNDC(); lx.SetTextFont(62); lx.SetTextSize(0.035); lx.SetTextColor(kBlack); lx.DrawLatex(0.37,0.29,Form("Required(before): %.0f%%/#sqrt{E}+5%%",oldA*100)); lx.SetTextColor(kBlue); lx.DrawLatex(0.41,0.22,Form("Required(now): %.0f%%/#sqrt{E}+5%%",newA*100)); }

TCanvas* makeResBiasCanvas(const string& particle, const string& cname, const string& title, std::map<string,TGraphErrors*>& res, std::map<string,TGraphErrors*>& bias, double xmax) {
    bool gamma=(particle=="gamma");
    TCanvas* c=new TCanvas(cname.c_str(),title.c_str(),1600,720); c->Divide(2,1);
    c->cd(1); stylePad();
    TMultiGraph* mgR=new TMultiGraph(Form("mgR_%s",cname.c_str()),"Energy Resolution");
    TLegend* leg=new TLegend(0.48,0.68,0.98,0.96); leg->SetFillColor(kWhite); leg->SetBorderSize(1); leg->SetTextSize(0.030);
    for(auto cv:curves){ string key=Form("m%d_w%d",cv.method,cv.weight); if(!res.count(key)) continue; styleGraph(res[key],cv.color,cv.marker); mgR->Add(res[key],"PL"); leg->AddEntry(res[key],Form("%s, %s",WEIGHT_LABEL[cv.weight],METHOD_LABEL[cv.method]),"pl"); }
    mgR->Draw("AP"); mgR->GetXaxis()->SetTitle("Energy (GeV)"); mgR->GetYaxis()->SetTitle("Energy Resolution (%)"); mgR->GetXaxis()->SetLimits(0,xmax); mgR->GetYaxis()->SetRangeUser(0,0.30); mgR->GetXaxis()->SetTitleSize(0.052); mgR->GetYaxis()->SetTitleSize(0.052); mgR->GetXaxis()->SetLabelSize(0.045); mgR->GetYaxis()->SetLabelSize(0.045); mgR->GetYaxis()->SetTitleOffset(1.15); drawReq(particle,1,xmax); leg->Draw();
    c->cd(2); stylePad();
    TMultiGraph* mgB=new TMultiGraph(Form("mgB_%s",cname.c_str()),"Energy Bias");
    TLegend* legB=new TLegend(0.48,0.68,0.98,0.96); legB->SetFillColor(kWhite); legB->SetBorderSize(1); legB->SetTextSize(0.030);
    for(auto cv:curves){ string key=Form("m%d_w%d",cv.method,cv.weight); if(!bias.count(key)) continue; styleGraph(bias[key],cv.color,cv.marker); mgB->Add(bias[key],"PL"); legB->AddEntry(bias[key],Form("%s, %s",WEIGHT_LABEL[cv.weight],METHOD_LABEL[cv.method]),"pl"); }
    mgB->Draw("AP"); mgB->GetXaxis()->SetTitle("Energy (GeV)"); mgB->GetYaxis()->SetTitle("E_{rec}/E_{beam}"); mgB->GetXaxis()->SetLimits(0,xmax); mgB->GetYaxis()->SetRangeUser(0.80,gamma?1.20:1.50); mgB->GetXaxis()->SetTitleSize(0.052); mgB->GetYaxis()->SetTitleSize(0.052); mgB->GetXaxis()->SetLabelSize(0.045); mgB->GetYaxis()->SetLabelSize(0.045); mgB->GetYaxis()->SetTitleOffset(1.15); TLine* one=new TLine(0,1,xmax,1); one->SetLineStyle(2); one->Draw("SAME"); legB->Draw();
    c->Write();
    return c;
}

TGraphErrors* dumpGraph(const vector<Sample>& samples,int what,const char* name){ TGraphErrors* g=new TGraphErrors(samples.size()); g->SetName(name); for(int i=0;i<(int)samples.size();++i){ double s=0,s2=0,n=0; for(auto&e:samples[i].events){ double num=(what==0?e.ecal:(what==1?e.hcal:e.ecal+e.hcal)); double v=num/e.beam; s+=v; s2+=v*v; n++; } double mean=s/TMath::Max(1.0,n), err=(n>1?std::sqrt(TMath::Max(0.0,s2/n-mean*mean))/std::sqrt(n):0); g->SetPoint(i,samples[i].beamMean,mean); g->SetPointError(i,0,err);} styleGraph(g,kBlue+2,kOpenCircle); return g; }
void drawDump(TGraphErrors*g,const char* title,const char*y,double xmax,double yhi){ stylePad(); g->SetTitle(title); g->Draw("AP"); g->GetXaxis()->SetTitle("Beam (GeV)"); g->GetYaxis()->SetTitle(y); g->GetXaxis()->SetLimits(0,xmax); g->GetYaxis()->SetRangeUser(0,yhi); }
TCanvas* makeEnergyDump(const vector<Sample>& gs,const vector<Sample>& ns){ TCanvas*c=new TCanvas("c_energy_dump","Energy Dump",1600,900); c->Divide(3,2); c->cd(1); drawDump(dumpGraph(gs,0,"gDump_gamma_ecal"),"Beam VS ECAL/Beam","ECAL / Beam",50,0.65); c->cd(2); drawDump(dumpGraph(gs,1,"gDump_gamma_hcal"),"Beam VS HCAL/Beam","HCAL / Beam",50,0.021); c->cd(3); drawDump(dumpGraph(gs,2,"gDump_gamma_all"),"Beam VS (ECAL + HCAL)/Beam","(ECAL+HCAL)/Beam",50,0.65); c->cd(4); drawDump(dumpGraph(ns,0,"gDump_neutron_ecal"),"Beam VS ECAL/Beam","ECAL / Beam",350,0.07); c->cd(5); drawDump(dumpGraph(ns,1,"gDump_neutron_hcal"),"Beam VS HCAL/Beam","HCAL / Beam",350,0.022); c->cd(6); drawDump(dumpGraph(ns,2,"gDump_neutron_all"),"Beam VS (ECAL + HCAL)/Beam","(ECAL+HCAL)/Beam",350,0.07); c->Write(); return c; }

void processOneMode(const string& tag,const string& particle,const vector<Sample>& train,const vector<Sample>& eval,int p0mode,TDirectory* dir,std::ofstream& pcsv,std::ofstream& mcsv,std::map<string,TGraphErrors*>& res,std::map<string,TGraphErrors*>& bias){ if(train.empty()||eval.empty()) return; dir->cd(); for(auto cv:curves){ Model model=fitModel(train,cv.method,cv.weight,p0mode,tag+"_"+particle,pcsv); vector<Metric> ms=metricsFor(eval,model,tag,particle,mcsv); string key=Form("m%d_w%d",cv.method,cv.weight); TGraphErrors* gr=graphFrom(ms,true,Form("gRes_%s_%s_%s",particle.c_str(),tag.c_str(),key.c_str())); TGraphErrors* gb=graphFrom(ms,false,Form("gBias_%s_%s_%s",particle.c_str(),tag.c_str(),key.c_str())); res[key]=gr; bias[key]=gb; dir->cd(); gr->Write(); gb->Write(); for(auto&m:ms) if(m.hist) m.hist->Write(); } }

void writeInputCsv(const char* qa,const vector<Sample>& g,const vector<Sample>& n){ std::ofstream out(Form("%s/input_summary.csv",qa)); out<<"particle,file,beam_mean_GeV,beam_rms_GeV,events,mean_ecal_over_beam,mean_hcal_over_beam,mean_total_over_beam,main_graph_used\n"; auto wr=[&](const Sample&s){ double a=0,b=0,c=0; for(auto&e:s.events){a+=e.ecal/e.beam;b+=e.hcal/e.beam;c+=(e.ecal+e.hcal)/e.beam;} double N=TMath::Max(1,(int)s.events.size()); bool main=(s.particle=="gamma"?(s.beamMean>=GAMMA_MAIN_MIN_GEV&&s.beamMean<=GAMMA_MAIN_MAX_GEV):(s.beamMean>=NEUTRON_EVAL_MIN_GEV&&s.beamMean<=NEUTRON_EVAL_MAX_GEV)); out<<s.particle<<","<<s.basename<<","<<s.beamMean<<","<<s.beamRMS<<","<<s.events.size()<<","<<a/N<<","<<b/N<<","<<c/N<<","<<(main?1:0)<<"\n";}; for(auto&s:g)wr(s); for(auto&s:n)wr(s); }

void zdc_make_graphs_v5(const char* inputDir="data", const char* outDir="plots", const char* qaDir="qa"){
    setStyle(); ensureDir(outDir); ensureDir(qaDir);
    vector<Sample> gammaAll, neutronAll; loadAll(inputDir,"gamma",gammaAll); loadAll(inputDir,"neutron",neutronAll); writeInputCsv(qaDir,gammaAll,neutronAll);
    vector<Sample> gammaMain=filterE(gammaAll,GAMMA_MAIN_MIN_GEV,GAMMA_MAIN_MAX_GEV);
    vector<Sample> neutronEval=filterE(neutronAll,NEUTRON_EVAL_MIN_GEV,NEUTRON_EVAL_MAX_GEV);
    vector<Sample> neutronTrain=filterE(neutronAll,0.0,NEUTRON_EVAL_MAX_GEV); // preserve old macro behavior: 10 GeV can help train, but not main eval.
    printf("[WINDOW] gamma main train/eval [1,40] = %zu / all %zu\n",gammaMain.size(),gammaAll.size());
    printf("[WINDOW] neutron train all <=300 = %zu, eval [20,300] = %zu / all %zu\n",neutronTrain.size(),neutronEval.size(),neutronAll.size());

    std::ofstream pcsv(Form("%s/regression_params.csv",qaDir));
    pcsv<<"tag,p0mode,method,weight,nUsed,p0,raw_c1,raw_c2,raw_c3,raw_c4\n";
    std::ofstream mcsv(Form("%s/regression_metrics.csv",qaDir));
    mcsv<<"tag,particle,p0mode,method,weight,energy_GeV,nEvents,bias_mu,reso_sigma,bias_err,reso_err,fit_ok\n";

    TFile* fout=TFile::Open(Form("%s/energy_reconstruction_graphs_v5.root",outDir),"RECREATE");
    TDirectory* dMain=fout->mkdir("01_main_graphs"); dMain->cd();
    TCanvas* dump=makeEnergyDump(gammaMain,neutronEval); dump->SaveAs(Form("%s/energy_dump.png",outDir));

    int modes[3]={0,1,2};
    for(int im=0;im<3;++im){ int p0mode=modes[im];
        std::map<string,TGraphErrors*> gr,gb,nr,nb;
        TDirectory* dg=fout->mkdir(Form("02_gamma_%s",p0Name(p0mode)));
        processOneMode(Form("main_%s",p0Name(p0mode)),"gamma",gammaMain,gammaMain,p0mode,dg,pcsv,mcsv,gr,gb);
        TDirectory* dn=fout->mkdir(Form("03_neutron_%s",p0Name(p0mode)));
        processOneMode(Form("main_%s",p0Name(p0mode)),"neutron",neutronTrain,neutronEval,p0mode,dn,pcsv,mcsv,nr,nb);
        fout->cd("01_main_graphs");
        if(!gammaMain.empty()){ TCanvas*c=makeResBiasCanvas("gamma",Form("c_gamma_resolution_bias_%s",p0Name(p0mode)),Form("Gamma Beam Energy Regression (%s)",p0Name(p0mode)),gr,gb,42); c->SaveAs(Form("%s/gamma_resolution_bias_%s.png",outDir,p0Name(p0mode))); }
        if(!neutronEval.empty()){ TCanvas*c=makeResBiasCanvas("neutron",Form("c_neutron_resolution_bias_%s",p0Name(p0mode)),Form("Neutron Beam Energy Regression (%s)",p0Name(p0mode)),nr,nb,310); c->SaveAs(Form("%s/neutron_resolution_bias_%s.png",outDir,p0Name(p0mode))); }
    }

    // All data QA: this keeps MeV gamma and any lower-energy neutron outputs without contaminating main plots.
    TDirectory* qaAll=fout->mkdir("04_QA_all_raw_data_kept"); qaAll->cd();
    // raw data summary graphs only; regression all-data can be added if needed, but the main CSV keeps every file.
    fout->Write(); fout->Close(); pcsv.close(); mcsv.close();
    printf("[DONE] %s/energy_reconstruction_graphs_v5.root\n",outDir);
    printf("[DONE] compare p0free, p0zero, p0ridge PNGs in %s/\n",outDir);
}
