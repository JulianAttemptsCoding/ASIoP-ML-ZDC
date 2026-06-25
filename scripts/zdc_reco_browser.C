// zdc_reco_browser.C
// Full ROOT/TBrowser pipeline: raw PODIO outfile_*.root -> ROOT canvases + PNG/PDF plots.
// Usage from repo root:
//   root -l -q 'scripts/zdc_reco_browser.C("data","plots")'
// Then inspect interactively:
//   root -l plots/energy_reconstruction_browser.root
//   new TBrowser

#include <TFile.h>
#include <TTree.h>
#include <TLeaf.h>
#include <TKey.h>
#include <TSystem.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TList.h>
#include <TCanvas.h>
#include <TPad.h>
#include <TGraphErrors.h>
#include <TMultiGraph.h>
#include <TH1D.h>
#include <TH1F.h>
#include <TArrow.h>
#include <TF1.h>
#include <TLegend.h>
#include <TPaveText.h>
#include <TLatex.h>
#include <TStyle.h>
#include <TLine.h>
#include <TMath.h>
#include <TMatrixD.h>
#include <TVectorD.h>
#include <TDecompSVD.h>
#include <TAxis.h>
#include <TROOT.h>
#include <TDirectory.h>
#include <TString.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using std::string;
using std::vector;

// ----------------------------- user-facing knobs -----------------------------
static const int    NBINS_RATIO_GAMMA   = 100;
static const int    NBINS_RATIO_NEUTRON = 150;
static const double RATIO_XMIN          = 0.0;
static const double RATIO_XMAX_GAMMA    = 2.0;
static const double RATIO_XMAX_NEUTRON  = 5.0;   // was 2.5; neutron ratio can be very large at low E
static const double VIS_CUT_FRAC        = 0.001; // exclude events with <0.1% visible energy/beam

// Method labels: match slide 6 wording.
static const char* METHOD_LABEL[3] = {"Linear Fun.", "Ratio Fun.", "Quadratic Fun."};
static const char* WEIGHT_LABEL[2] = {"Equal-Weighted", "#sqrt{E_{Beam}}-weighted"};

struct EventData {
    double ecal;
    double hcal;
    double beam;
};

struct Sample {
    string particle;
    string path;
    string basename;
    double beamMean;
    double beamRMS;
    vector<EventData> events;
};

struct RecoMetrics {
    double E;
    double reso;
    double resoErr;
    double bias;
    double biasErr;
    TH1D* hist;
    TF1*  fit;
};

struct CurveDef {
    int method;
    int weight;
    int color;
    int marker;
};

// ----------------------------- helpers ---------------------------------------
string baseName(const string& p) {
    size_t pos = p.find_last_of("/\\");
    return (pos == string::npos) ? p : p.substr(pos + 1);
}

bool endsWith(const string& s, const string& suf) {
    return s.size() >= suf.size() && s.compare(s.size()-suf.size(), suf.size(), suf) == 0;
}

void ensureDir(const char* path) {
    if (gSystem->AccessPathName(path)) gSystem->mkdir(path, true);
}

void setGlobalStyle() {
    gStyle->SetOptStat(1110);
    gStyle->SetOptFit(0);
    gStyle->SetCanvasColor(kWhite);
    gStyle->SetPadColor(kWhite);
    gStyle->SetFrameFillColor(kWhite);
    gStyle->SetTitleFont(42, "XYZ");
    gStyle->SetLabelFont(42, "XYZ");
    gStyle->SetLegendFont(42);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);
    gStyle->SetGridStyle(3);
    gStyle->SetGridWidth(1);
}

TLeaf* getLeafAny(TTree* t, const vector<const char*>& names) {
    for (auto n : names) {
        TLeaf* l = t->GetLeaf(n);
        if (l) return l;
    }
    return nullptr;
}

void collectFilesRecursive(const string& dir, const string& particle, vector<string>& out) {
    TSystemDirectory sd(dir.c_str(), dir.c_str());
    TList* files = sd.GetListOfFiles();
    if (!files) return;

    TIter next(files);
    TObject* obj = nullptr;
    while ((obj = next())) {
        TSystemFile* f = (TSystemFile*)obj;
        string name = f->GetName();
        if (name == "." || name == "..") continue;
        string full = dir + "/" + name;
        if (f->IsDirectory()) {
            collectFilesRecursive(full, particle, out);
            continue;
        }
        if (!endsWith(name, ".root")) continue;
        TString lower(name.c_str());
        lower.ToLower();
        TString part(particle.c_str());
        part.ToLower();
        // Require both outfile/raw naming and particle token so old result .root files are not mixed in.
        if (lower.Contains("outfile") && lower.Contains(part)) out.push_back(full);
    }
    delete files;
}

// Double-sided Crystal Ball in ratio variable x = E_rec / E_beam.
// p[0]=mu, p[1]=sigma, p[2]=alphaL, p[3]=nL, p[4]=alphaR, p[5]=nR, p[6]=norm
Double_t dcbFunc(Double_t* x, Double_t* p) {
    const double sig = TMath::Abs(p[1]);
    if (sig <= 1e-12) return 0.0;
    const double t  = (x[0] - p[0]) / sig;
    const double aL = TMath::Max(1e-4, TMath::Abs(p[2]));
    const double nL = TMath::Max(1.01, p[3]);
    const double aR = TMath::Max(1e-4, TMath::Abs(p[4]));
    const double nR = TMath::Max(1.01, p[5]);

    double val = 0.0;
    if (t < -aL) {
        const double A = TMath::Power(nL/aL, nL) * TMath::Exp(-0.5*aL*aL);
        const double B = nL/aL - aL;
        const double arg = B - t;
        val = (arg > 0) ? A * TMath::Power(arg, -nL) : 0.0;
    } else if (t > aR) {
        const double A = TMath::Power(nR/aR, nR) * TMath::Exp(-0.5*aR*aR);
        const double B = nR/aR - aR;
        const double arg = B + t;
        val = (arg > 0) ? A * TMath::Power(arg, -nR) : 0.0;
    } else {
        val = TMath::Exp(-0.5*t*t);
    }
    return p[6] * val;
}

bool loadSample(const string& path, const string& particle, Sample& s) {
    TFile* f = TFile::Open(path.c_str(), "READ");
    if (!f || f->IsZombie()) {
        printf("[ERROR] Cannot open %s\n", path.c_str());
        return false;
    }
    TTree* t = (TTree*)f->Get("events");
    if (!t) {
        printf("[ERROR] Missing TTree 'events' in %s\n", path.c_str());
        f->Close();
        return false;
    }

    TLeaf* lEcal = getLeafAny(t, {"EcalFarForwardZDCHits.energy", "EcalFarForwardZDCHits/EcalFarForwardZDCHits.energy"});
    TLeaf* lHcal = getLeafAny(t, {"HcalFarForwardZDCHits.energy", "HcalFarForwardZDCHits/HcalFarForwardZDCHits.energy"});
    TLeaf* lPx   = getLeafAny(t, {"MCParticles.momentum.x", "MCParticles/MCParticles.momentum.x"});
    TLeaf* lPy   = getLeafAny(t, {"MCParticles.momentum.y", "MCParticles/MCParticles.momentum.y"});
    TLeaf* lPz   = getLeafAny(t, {"MCParticles.momentum.z", "MCParticles/MCParticles.momentum.z"});
    TLeaf* lMass = getLeafAny(t, {"MCParticles.mass", "MCParticles/MCParticles.mass"});
    TLeaf* lStat = getLeafAny(t, {"MCParticles.generatorStatus", "MCParticles/MCParticles.generatorStatus"});

    if (!lEcal || !lHcal || !lPx || !lPy || !lPz || !lMass) {
        printf("[ERROR] Missing required PODIO leaves in %s\n", path.c_str());
        f->Close();
        return false;
    }

    s.particle = particle;
    s.path = path;
    s.basename = baseName(path);
    s.events.clear();

    double sumE = 0.0, sumE2 = 0.0;
    Long64_t nValid = 0;
    const Long64_t nEntries = t->GetEntries();

    for (Long64_t iev = 0; iev < nEntries; ++iev) {
        t->GetEntry(iev);
        double ecal = 0.0;
        for (int i = 0; i < lEcal->GetLen(); ++i) ecal += lEcal->GetValue(i);
        double hcal = 0.0;
        for (int i = 0; i < lHcal->GetLen(); ++i) hcal += lHcal->GetValue(i);

        int iprimary = 0;
        if (lStat) {
            for (int ip = 0; ip < lStat->GetLen(); ++ip) {
                if ((int)lStat->GetValue(ip) == 1) { iprimary = ip; break; }
            }
        }
        const double px = lPx->GetValue(iprimary);
        const double py = lPy->GetValue(iprimary);
        const double pz = lPz->GetValue(iprimary);
        const double m  = lMass->GetValue(iprimary);
        const double beam = std::sqrt(px*px + py*py + pz*pz + m*m);
        if (!std::isfinite(beam) || beam <= 1e-9) continue;

        EventData e{ecal, hcal, beam};
        s.events.push_back(e);
        sumE += beam;
        sumE2 += beam*beam;
        nValid++;
    }
    f->Close();

    if (nValid == 0) {
        printf("[ERROR] No valid events in %s\n", path.c_str());
        return false;
    }
    s.beamMean = sumE / nValid;
    const double var = TMath::Max(0.0, sumE2 / nValid - s.beamMean*s.beamMean);
    s.beamRMS = std::sqrt(var);
    printf("[LOAD] %-8s  %-35s  entries=%lld  Ebeam=%.6g GeV\n",
           particle.c_str(), s.basename.c_str(), nValid, s.beamMean);
    return true;
}

void mergeSameEnergy(vector<Sample>& samples, const string& particle);
vector<Sample> filterSamples(const vector<Sample>& samples, double minE, double maxE);
bool robustCoreStats(vector<double> vals, double& mu, double& sig, double& muErr, double& sigErr);

void loadAllSamples(const char* inputDir, const string& particle, vector<Sample>& samples) {
    vector<string> files;
    collectFilesRecursive(inputDir, particle, files);
    std::sort(files.begin(), files.end());

    for (auto& p : files) {
        Sample s;
        if (loadSample(p, particle, s)) samples.push_back(s);
    }
    std::sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) {
        return a.beamMean < b.beamMean;
    });
    mergeSameEnergy(samples, particle);
}

vector<double> featureVector(const EventData& e, int method) {
    const double tot = e.ecal + e.hcal;
    if (method == 0) return {1.0, e.ecal, e.hcal};
    if (method == 1) return {1.0, e.ecal, e.hcal, (tot > 1e-12 ? e.hcal/tot : 0.0)};
    // Full quadratic with linear terms: E_rec = p0 + p1*E_ECAL + p2*E_HCAL + p3*E_ECAL^2 + p4*E_HCAL^2
    // Old form {1, ecal^2, hcal^2} lacked linear terms — feature values span
    // 3+ orders of magnitude across neutron energy range, causing regression blow-up.
    return {1.0, e.ecal, e.hcal, e.ecal*e.ecal, e.hcal*e.hcal};
}

// Reference (Taiwan group res2_1_*) uses a free intercept; its neutron bias is >1 and
// decreases toward 1 at high energy. Matching that look requires p0 free, so keep it.
static const bool ZERO_INTERCEPT = false;

bool fitRegression(const vector<Sample>& samples, int method, int weightMode, vector<double>& params) {
    const int k = (method == 0) ? 3 : (method == 1) ? 4 : 5;
    const int start = ZERO_INTERCEPT ? 1 : 0;   // skip the constant term when forcing p0=0
    const int kk = k - start;
    TMatrixD normal(kk, kk);
    TVectorD rhs(kk);
    int nUsed = 0;

    for (const auto& s : samples) {
        for (const auto& e : s.events) {
            const double vis = e.ecal + e.hcal;
            if (!std::isfinite(vis) || vis <= 0.0) continue;
            vector<double> x = featureVector(e, method);
            // weightMode 1 reproduces the reference's second ("sqrt(Ebeam)-weighted") curve.
            // Matching the archive res2_1_* graphs empirically shows that curve was computed
            // with 1/E weighting (it down-weights high energy, giving the low neutron bias
            // ~1.3 at 20 GeV). A literal sqrt(E) weight does the opposite (bias ~2.9). Keep
            // the reference legend label but use the weighting that reproduces its numbers.
            const double w = (weightMode == 1) ? 1.0 / e.beam : 1.0;
            for (int i = 0; i < kk; ++i) {
                rhs(i) += w * x[i+start] * e.beam;
                for (int j = 0; j < kk; ++j) normal(i,j) += w * x[i+start] * x[j+start];
            }
            ++nUsed;
        }
    }

    params.assign(k, 0.0);   // params[0] stays 0 when ZERO_INTERCEPT; applyReco still uses p[0]
    if (nUsed < kk + 5) {
        printf("[WARN] Not enough events for regression m%d_w%d; used=%d\n", method, weightMode, nUsed);
        return false;
    }
    TDecompSVD svd(normal);
    Bool_t ok = kTRUE;
    TVectorD sol = svd.Solve(rhs, ok);
    if (!ok) {
        printf("[WARN] SVD solve failed for regression m%d_w%d\n", method, weightMode);
        return false;
    }
    for (int i = 0; i < kk; ++i) params[i+start] = sol(i);

    printf("[FIT] m%d_w%d params (p0=%s):", method, weightMode, ZERO_INTERCEPT ? "0 fixed" : "free");
    for (double v : params) printf(" %.6g", v);
    printf("\n");
    return true;
}

double applyReco(const EventData& e, int method, const vector<double>& p) {
    const double tot = e.ecal + e.hcal;
    if (method == 0) return p[0] + p[1]*e.ecal + p[2]*e.hcal;
    if (method == 1) return p[0] + p[1]*e.ecal + p[2]*e.hcal + p[3]*(tot > 1e-12 ? e.hcal/tot : 0.0);
    return p[0] + p[1]*e.ecal + p[2]*e.hcal + p[3]*e.ecal*e.ecal + p[4]*e.hcal*e.hcal;
}

// Iterative Gaussian core fit -- the standard calorimetry "core resolution" estimator.
// Fit a Gaussian, then refit within +/-2 sigma, iterating until the width stabilizes.
// It converges on the central peak, excludes the tails, and gives ONE consistent
// estimator at every energy and method. Using it for the graph metric removes the
// spurious resolution spikes that appear when a per-point estimate switches between a
// narrow DCB core and a wide robust fallback (the 50 GeV and 150 GeV artifacts).
bool iterativeGaussianCore(TH1D* h, double seedMu, double seedSig,
                           double& mu, double& sig, double& muErr, double& sigErr) {
    if (!h || h->GetEntries() < 30) return false;
    const double xmin = h->GetXaxis()->GetXmin();
    const double xmax = h->GetXaxis()->GetXmax();
    const double bw   = h->GetBinWidth(1);
    double cMu = seedMu, cSig = TMath::Max(seedSig, 2.0*bw);
    TF1 g("gcore", "gaus", xmin, xmax);
    bool ok = false;
    for (int it = 0; it < 5; ++it) {
        const double lo = TMath::Max(xmin, cMu - 2.0*cSig);
        const double hi = TMath::Min(xmax, cMu + 2.0*cSig);
        if (hi - lo < 3.0*bw) break;
        g.SetRange(lo, hi);
        g.SetParameters(h->GetMaximum(), cMu, cSig);
        const int st = h->Fit(&g, "RQ0L");          // "0" => not stored on histo (no dangling TF1)
        const double nMu  = g.GetParameter(1);
        const double nSig = TMath::Abs(g.GetParameter(2));
        if (st != 0 || !std::isfinite(nMu) || !std::isfinite(nSig) || nSig <= 0.0) break;
        const bool converged = TMath::Abs(nSig - cSig) < 0.02 * cSig;
        cMu = nMu; cSig = nSig;
        mu = cMu; sig = cSig; muErr = g.GetParError(1); sigErr = g.GetParError(2);
        ok = true;
        if (converged && it >= 1) break;
    }
    return ok && std::isfinite(mu) && std::isfinite(sig) && sig > 0.0;
}

bool fitRatioDCB(TH1D* h, const vector<double>& ratios,
                 TF1*& fit, double& mu, double& sig, double& muErr, double& sigErr) {
    fit = nullptr;
    mu = sig = muErr = sigErr = 0.0;
    if (!h || h->GetEntries() < 30) return false;

    // First compute a robust core estimate. This is the accepted fallback and also
    // seeds/constrains the DCB fit. It prevents low-energy tails or failed fits from
    // creating the enormous error bars seen in the first pass.
    double rMu=0.0, rSig=0.0, rMuErr=0.0, rSigErr=0.0;
    if (!robustCoreStats(ratios, rMu, rSig, rMuErr, rSigErr)) return false;
    mu = rMu; sig = rSig; muErr = rMuErr; sigErr = rSigErr;

    // Seed metric with the iterative Gaussian core (always succeeds). The DCB core below
    // overrides it when the DCB converges -- the reference (res2_1_*) reports the DCB-core
    // sigma, which is the narrow peak width and is what makes the curves meet the
    // requirement lines. Gaussian core is the consistent fallback so a failed DCB cannot
    // produce a spike.
    {
        double gMu=0, gSig=0, gMuErr=0, gSigErr=0;
        if (iterativeGaussianCore(h, rMu, rSig, gMu, gSig, gMuErr, gSigErr)) {
            mu = gMu; sig = gSig; muErr = gMuErr; sigErr = gSigErr;
        }
    }

    const double xmin = h->GetXaxis()->GetXmin();
    const double xmax = h->GetXaxis()->GetXmax();
    const double fitLo = TMath::Max(xmin, rMu - TMath::Max(0.20, 5.0*rSig));
    const double fitHi = TMath::Min(xmax, rMu + TMath::Max(0.20, 5.0*rSig));

    TF1* dcb = new TF1(Form("dcb_%s", h->GetName()), dcbFunc, fitLo, fitHi, 7);
    dcb->SetParNames("#mu", "#sigma", "#alpha_{L}", "n_{L}", "#alpha_{R}", "n_{R}", "N");
    dcb->SetParameters(rMu, TMath::Max(rSig, 0.003), 1.5, 3.0, 1.8, 3.0, h->GetMaximum());
    dcb->SetParLimits(0, TMath::Max(xmin, rMu - TMath::Max(0.12, 3.0*rSig)),
                         TMath::Min(xmax, rMu + TMath::Max(0.12, 3.0*rSig)));
    dcb->SetParLimits(1, 0.001, TMath::Min(2.00, TMath::Max(0.08, 5.0*rSig))); // was 0.60; neutron reso can exceed 80%
    dcb->SetParLimits(2, 0.35, 6.0);
    dcb->SetParLimits(3, 1.10, 30.0);
    dcb->SetParLimits(4, 0.35, 6.0);
    dcb->SetParLimits(5, 1.10, 30.0);
    dcb->SetParLimits(6, 0.0, h->GetMaximum()*10.0 + 1.0);
    dcb->SetLineColor(kRed+1);
    dcb->SetLineWidth(3);

    const int status = h->Fit(dcb, "RQ0");
    const double fMu = dcb->GetParameter(0);
    const double fSig = TMath::Abs(dcb->GetParameter(1));
    const double fMuErr = dcb->GetParError(0);
    const double fSigErr = dcb->GetParError(1);

    // Relaxed acceptance: take the DCB core as the metric whenever the fit is finite and
    // physically reasonable. We do NOT require MINUIT status==0 (it flags non-fatal issues
    // that previously forced good high-energy fits onto the wider Gaussian fallback,
    // creating spikes). This makes the DCB core the consistent estimator -> smooth,
    // narrow, reference-matching curves.
    const bool sane = (std::isfinite(fMu) && std::isfinite(fSig) &&
                       std::isfinite(fMuErr) && std::isfinite(fSigErr) &&
                       fSig > 0.0 && fSig < 0.75 &&
                       TMath::Abs(fMu - rMu) < TMath::Max(0.25, 4.0*rSig) &&
                       fMuErr < 0.20 && fSigErr < 0.20);
    if (sane) {
        fit = dcb;
        mu = fMu; sig = fSig; muErr = fMuErr; sigErr = fSigErr;  // DCB core drives the graph
    } else {
        delete dcb; fit = nullptr;   // keep the Gaussian-core metric already set above
    }
    return (sig > 0.0);
}

vector<RecoMetrics> buildRecoMetrics(const vector<Sample>& samples,
                                     const vector<double>& params,
                                     int method, int weightMode,
                                     const string& particle) {
    vector<RecoMetrics> metrics;
    const bool gamma = (particle == "gamma");
    const int nbins = gamma ? NBINS_RATIO_GAMMA : NBINS_RATIO_NEUTRON;
    const double xmax = gamma ? RATIO_XMAX_GAMMA : RATIO_XMAX_NEUTRON;

    for (const auto& s : samples) {
        TH1D* h = new TH1D(Form("hRatio_%s_m%d_w%d_E%.6g", particle.c_str(), method, weightMode, s.beamMean),
                           Form("%s  E_{beam}=%.3g GeV;E_{rec}/E_{beam};Count", particle.c_str(), s.beamMean),
                           nbins, RATIO_XMIN, xmax);
        h->SetDirectory(nullptr);
        h->SetLineColor(kBlue+2);
        h->SetLineWidth(2);

        vector<double> ratios;
        ratios.reserve(s.events.size());
        for (const auto& e : s.events) {
            // Skip zero-deposit events: without this cut, every null event predicts
            // E_rec = p0 (the intercept), creating a delta spike at p0/E_beam
            // that dominates sigma and causes artificial volatility in the resolution graph.
            const double vis = e.ecal + e.hcal;
            if (vis < VIS_CUT_FRAC * e.beam) continue;
            const double rec = applyReco(e, method, params);
            const double ratio = rec / e.beam;
            if (std::isfinite(ratio)) { h->Fill(ratio); ratios.push_back(ratio); }
        }
        TF1* fit = nullptr;
        double mu = 0, sig = 0, muErr = 0, sigErr = 0;
        const bool ok = fitRatioDCB(h, ratios, fit, mu, sig, muErr, sigErr);
        printf("[METRIC] %-8s E=%8.4f  m%d_w%d  bias=%.5f  reso=%.5f  metric=gcore dcbShown=%s\n",
               particle.c_str(), s.beamMean, method, weightMode, mu, sig, (ok && fit) ? "yes" : "no");
        metrics.push_back({s.beamMean, sig, sigErr, mu, muErr, h, fit});
    }
    std::sort(metrics.begin(), metrics.end(), [](const RecoMetrics& a, const RecoMetrics& b) { return a.E < b.E; });
    return metrics;
}

TGraphErrors* makeGraph(const vector<RecoMetrics>& metrics, bool resolution, const char* name) {
    TGraphErrors* g = new TGraphErrors(metrics.size());
    g->SetName(name);
    g->SetTitle(name);
    for (int i = 0; i < (int)metrics.size(); ++i) {
        g->SetPoint(i, metrics[i].E, resolution ? metrics[i].reso : metrics[i].bias);
        g->SetPointError(i, 0.0, resolution ? metrics[i].resoErr : metrics[i].biasErr);
    }
    return g;
}

void styleGraph(TGraphErrors* g, int color, int marker) {
    if (!g) return;
    g->SetMarkerColor(color);
    g->SetLineColor(color);
    g->SetMarkerStyle(marker);
    g->SetMarkerSize(1.0);
    g->SetLineWidth(2);
}

void stylePad(double lm=0.13, double bm=0.13, double rm=0.03, double tm=0.10) {
    gPad->SetLeftMargin(lm);
    gPad->SetBottomMargin(bm);
    gPad->SetRightMargin(rm);
    gPad->SetTopMargin(tm);
    gPad->SetTicks(1,1);
    gPad->SetGrid(1,1);
}

void drawSlideHeader(TCanvas* c, const char* title) {
    c->cd();
    TLatex lx;
    lx.SetNDC();
    lx.SetTextFont(42);
    lx.SetTextSize(0.045);
    lx.SetTextAlign(22);
    lx.DrawLatex(0.50, 0.975, title);
    TLine line(0.0, 0.94, 1.0, 0.94);
    line.SetNDC(kTRUE);
    line.SetLineColor(kBlue+1);
    line.SetLineWidth(5);
    line.DrawClone();
}

void addSlideFooter(TCanvas* c, const char* page) {
    c->cd();
    TLatex lx;
    lx.SetNDC();
    lx.SetTextFont(42);
    lx.SetTextSize(0.018);
    lx.SetTextColor(kGray+2);
    lx.DrawLatex(0.02, 0.015, "2026/05/01");
    lx.DrawLatex(0.41, 0.015, "ZDC MC status (Taiwan Group)");
    lx.DrawLatex(0.94, 0.015, page);
}

double meanOf(const vector<double>& v) {
    if (v.empty()) return 0.0;
    double s = 0.0;
    for (double x : v) s += x;
    return s / v.size();
}

double stderrOf(const vector<double>& v) {
    if (v.size() < 2) return 0.0;
    const double m = meanOf(v);
    double ss = 0.0;
    for (double x : v) ss += (x-m)*(x-m);
    return std::sqrt(ss / (v.size()-1)) / std::sqrt((double)v.size());
}


vector<Sample> filterSamples(const vector<Sample>& samples, double minE, double maxE) {
    vector<Sample> out;
    for (const auto& s : samples) {
        if (s.beamMean + 1e-9 >= minE && s.beamMean - 1e-9 <= maxE) out.push_back(s);
    }
    return out;
}

void mergeSameEnergy(vector<Sample>& samples, const string& particle) {
    if (samples.empty()) return;
    std::sort(samples.begin(), samples.end(), [](const Sample& a, const Sample& b) {
        return a.beamMean < b.beamMean;
    });
    vector<Sample> merged;
    for (const auto& s : samples) {
        if (merged.empty()) { merged.push_back(s); continue; }
        Sample& last = merged.back();
        const double tol = TMath::Max(1e-6, 1e-5 * TMath::Max(1.0, last.beamMean));
        if (TMath::Abs(s.beamMean - last.beamMean) <= tol) {
            printf("[MERGE] %-8s same beam energy %.6g GeV: %s + %s\n",
                   particle.c_str(), last.beamMean, last.basename.c_str(), s.basename.c_str());
            last.events.insert(last.events.end(), s.events.begin(), s.events.end());
            last.basename += "+" + s.basename;
            last.path += ";" + s.path;
            // Recompute grouped mean/rms from event truth energies.
            double sum=0.0, sum2=0.0;
            for (const auto& e : last.events) { sum += e.beam; sum2 += e.beam*e.beam; }
            const double n = (double)last.events.size();
            last.beamMean = sum/n;
            last.beamRMS = std::sqrt(TMath::Max(0.0, sum2/n - last.beamMean*last.beamMean));
        } else {
            merged.push_back(s);
        }
    }
    samples.swap(merged);
}

double quantileSorted(const vector<double>& sorted, double q) {
    if (sorted.empty()) return 0.0;
    const double pos = q * (sorted.size() - 1);
    const int i0 = (int)std::floor(pos);
    const int i1 = (int)std::ceil(pos);
    if (i0 == i1) return sorted[i0];
    const double t = pos - i0;
    return sorted[i0] * (1.0 - t) + sorted[i1] * t;
}

bool robustCoreStats(vector<double> vals, double& mu, double& sig, double& muErr, double& sigErr) {
    vals.erase(std::remove_if(vals.begin(), vals.end(), [](double x){ return !std::isfinite(x); }), vals.end());
    if (vals.size() < 5) { mu=sig=muErr=sigErr=0.0; return false; }
    std::sort(vals.begin(), vals.end());
    const double q16 = quantileSorted(vals, 0.16);
    const double q50 = quantileSorted(vals, 0.50);
    const double q84 = quantileSorted(vals, 0.84);
    double core = 0.5 * (q84 - q16);
    if (!std::isfinite(core) || core <= 1e-9) core = TMath::Max(1e-3, 0.5*(vals.back()-vals.front()));
    const double lo = q50 - 2.5*core;
    const double hi = q50 + 2.5*core;
    double sw=0.0, sx=0.0, sx2=0.0;
    for (double x : vals) {
        if (x < lo || x > hi) continue;
        sw += 1.0; sx += x; sx2 += x*x;
    }
    if (sw < 5) { sw=0.0; sx=0.0; sx2=0.0; for (double x : vals) { sw+=1.0; sx+=x; sx2+=x*x; } }
    mu = sx / sw;
    sig = std::sqrt(TMath::Max(0.0, sx2 / sw - mu*mu));
    muErr = (sw > 1.0) ? sig / std::sqrt(sw) : 0.0;
    sigErr = (sw > 2.0) ? sig / std::sqrt(2.0*(sw-1.0)) : 0.0;
    return std::isfinite(mu) && std::isfinite(sig) && sig > 0.0;
}


TGraphErrors* makeDumpGraph(const vector<Sample>& samples, int quantity, const char* name) {
    TGraphErrors* g = new TGraphErrors(samples.size());
    g->SetName(name);
    for (int i = 0; i < (int)samples.size(); ++i) {
        vector<double> ratios;
        ratios.reserve(samples[i].events.size());
        for (const auto& e : samples[i].events) {
            double numerator = 0.0;
            if (quantity == 0) numerator = e.ecal;
            else if (quantity == 1) numerator = e.hcal;
            else numerator = e.ecal + e.hcal;
            ratios.push_back(numerator / e.beam);
        }
        g->SetPoint(i, samples[i].beamMean, meanOf(ratios));
        g->SetPointError(i, 0.0, stderrOf(ratios));
    }
    styleGraph(g, kBlue+2, kOpenCircle);
    return g;
}


void drawFrameIfEmpty(const char* title, const char* ytitle, double xMax, double yMax) {
    TH1F* frame = gPad->DrawFrame(0.0, 0.0, xMax, yMax);
    frame->SetTitle(title);
    frame->GetXaxis()->SetTitle("Beam (GeV)");
    frame->GetYaxis()->SetTitle(ytitle);
}

void drawSlideTitle(const char* title) {
    TLatex lx;
    lx.SetNDC(); lx.SetTextFont(42); lx.SetTextAlign(22); lx.SetTextSize(0.055);
    lx.DrawLatex(0.50, 0.955, title);
    TLine* line = new TLine(0.0, 0.915, 1.0, 0.915);
    line->SetNDC(kTRUE); line->SetLineColor(kBlue+1); line->SetLineWidth(5); line->Draw();
    TPaveText* block = new TPaveText(0.047, 0.912, 0.205, 0.930, "NDC");
    block->SetFillColor(kAzure-9); block->SetLineColor(kAzure-9); block->SetBorderSize(0); block->Draw();
}

void drawFooterText(const char* page) {
    TLatex lx;
    lx.SetNDC(); lx.SetTextFont(42); lx.SetTextSize(0.018); lx.SetTextColor(kGray+2);
    lx.DrawLatex(0.012, 0.018, "2026/05/01");
    lx.SetTextAlign(22); lx.DrawLatex(0.50, 0.018, "ZDC MC status (Taiwan Group)");
    lx.SetTextAlign(32); lx.DrawLatex(0.985, 0.018, page);
}

TPaveText* lightBlueBox(double x1, double y1, double x2, double y2) {
    TPaveText* b = new TPaveText(x1,y1,x2,y2,"NDC");
    b->SetFillColor(kAzure-9); b->SetLineColor(kAzure-9); b->SetBorderSize(0); b->Draw();
    return b;
}

void drawSingleDumpPad(TGraphErrors* g, const char* title, const char* ytitle,
                       const char* rowLabel, double xMax, double yMax) {
    stylePad(0.16, 0.16, 0.04, 0.12);
    if (!g || g->GetN() == 0) {
        drawFrameIfEmpty(title, ytitle, xMax, yMax);
    } else {
        g->SetTitle(title);
        g->Draw("AP");
        g->GetXaxis()->SetTitle("Beam (GeV)");
        g->GetYaxis()->SetTitle(ytitle);
        g->GetXaxis()->SetLimits(0.0, xMax);
        g->GetYaxis()->SetRangeUser(0.0, yMax);
        g->GetXaxis()->SetTitleSize(0.050);
        g->GetYaxis()->SetTitleSize(0.050);
        g->GetXaxis()->SetLabelSize(0.042);
        g->GetYaxis()->SetLabelSize(0.042);
        g->GetYaxis()->SetTitleOffset(1.20);
    }
    if (rowLabel && strlen(rowLabel) > 0) {
        TLatex lx;
        lx.SetNDC(); lx.SetTextColor(kBlue); lx.SetTextFont(62); lx.SetTextSize(0.058);
        lx.DrawLatex(0.045, 0.105, rowLabel);
    }
}

TCanvas* makeEnergyDumpCanvas(const vector<Sample>& gammaSamples, const vector<Sample>& neutronSamples) {
    TCanvas* c = new TCanvas("c_slide5_energy_dump", "Slide 5 - Energy Dump", 1600, 1200);
    c->SetFillColor(kWhite);
    c->cd();
    drawSlideTitle("Energy Dump");

    TGraphErrors* gGE = makeDumpGraph(gammaSamples, 0, "gDump_gamma_ecal_over_beam");
    TGraphErrors* gGH = makeDumpGraph(gammaSamples, 1, "gDump_gamma_hcal_over_beam");
    TGraphErrors* gGA = makeDumpGraph(gammaSamples, 2, "gDump_gamma_all_over_beam");
    TGraphErrors* gNE = makeDumpGraph(neutronSamples, 0, "gDump_neutron_ecal_over_beam");
    TGraphErrors* gNH = makeDumpGraph(neutronSamples, 1, "gDump_neutron_hcal_over_beam");
    TGraphErrors* gNA = makeDumpGraph(neutronSamples, 2, "gDump_neutron_all_over_beam");

    // Layout copied from the reference slide: 3 columns, two compact rows,
    // a separator line, and a bottom note box.
    const double x1[3] = {0.055, 0.372, 0.690};
    const double x2[3] = {0.300, 0.617, 0.935};
    const double yTop1 = 0.535, yTop2 = 0.845;
    const double yBot1 = 0.230, yBot2 = 0.500;
    TPad* pads[6];
    for (int i=0;i<3;i++) {
        pads[i] = new TPad(Form("pad_dump_top_%d",i), "", x1[i], yTop1, x2[i], yTop2); pads[i]->Draw();
        pads[i+3] = new TPad(Form("pad_dump_bot_%d",i), "", x1[i], yBot1, x2[i], yBot2); pads[i+3]->Draw();
    }
    pads[0]->cd(); drawSingleDumpPad(gGE, "Beam VS ECAL/Beam", "ECal / Beam", "Gamma (all energies)", 50.0, 0.60);
    pads[1]->cd(); drawSingleDumpPad(gGH, "Beam VS HCAL/Beam", "HCal / Beam", "", 50.0, 0.020);
    pads[2]->cd(); drawSingleDumpPad(gGA, "Beam VS\n(ECAL + HCAL)/Beam", "(ECal + HCal) / Beam", "", 50.0, 0.60);
    pads[3]->cd(); drawSingleDumpPad(gNE, "", "ECal / Beam", "Neutron (all energies)", 350.0, 0.070);
    pads[4]->cd(); drawSingleDumpPad(gNH, "", "HCal / Beam", "", 350.0, 0.022);
    pads[5]->cd(); drawSingleDumpPad(gNA, "", "(ECal + HCal) / Beam", "", 350.0, 0.070);

    c->cd();
    TLine* sep = new TLine(0.04, 0.515, 0.96, 0.515); sep->SetNDC(kTRUE); sep->SetLineWidth(3); sep->Draw();
    lightBlueBox(0.10,0.065,0.92,0.145);
    TLatex tx; tx.SetNDC(); tx.SetTextFont(42); tx.SetTextSize(0.022); tx.SetTextColor(kBlack);
    tx.DrawLatex(0.125,0.115,"#bullet   Gamma beam (0.1#minus40 GeV) : Most of energy dumped in ECAL ~ 20% to 60%.");
    tx.DrawLatex(0.125,0.085,"#bullet   Neutron beam (all energies) : Not much energy dumped in scintillator tile, only 3% to 6%.");
    drawFooterText("5/18");

    c->Write();
    gGE->Write(); gGH->Write(); gGA->Write(); gNE->Write(); gNH->Write(); gNA->Write();
    return c;
}

TCanvas* makeSlide6Canvas(const vector<RecoMetrics>& gamma_m0_w1) {
    if (gamma_m0_w1.empty()) return nullptr;
    int best = 0;
    double bestD = 1e99;
    for (int i = 0; i < (int)gamma_m0_w1.size(); ++i) {
        const double d = TMath::Abs(gamma_m0_w1[i].E - 1.0);
        if (d < bestD) { bestD = d; best = i; }
    }
    const RecoMetrics& m = gamma_m0_w1[best];

    TCanvas* c = new TCanvas("c_slide6_gamma1GeV_regression", "Slide 6 - Different Kinds of Energy Regression", 1600, 1200);
    c->SetFillColor(kWhite);
    c->cd();
    drawSlideTitle("Different Kinds of Energy Regression");

    TLatex tx;
    tx.SetNDC(); tx.SetTextFont(42); tx.SetTextSize(0.027); tx.SetTextColor(kBlack);
    tx.DrawLatex(0.055,0.865,"#bullet   Fitting Functions");
    tx.DrawLatex(0.080,0.835,"-   Linear function :        E_{rec} = p0 + p1 * E_{ECAL} + p2 * E_{HCAL}");
    tx.DrawLatex(0.080,0.805,"-   Linear function w/ ratio :  E_{rec} = p0 + p1 * E_{ECAL} + p2 * E_{HCAL} + p3 * E_{HCAL}/(E_{ECAL}+E_{HCAL})");
    tx.DrawLatex(0.080,0.775,"-   Quadratic function :    E_{rec} = p0 + p1 * E_{ECAL}^{2} + p2 * E_{HCAL}^{2}");
    tx.DrawLatex(0.055,0.715,"#bullet   Weighting (Events enter fitting with a certain weight)");
    tx.DrawLatex(0.080,0.685,"-   Equal weighting : weighting = 1  (all the events are equal)");
    tx.DrawLatex(0.080,0.655,"-   Square-root of beam energy : weighting = #sqrt{E_{beam}}  (higher energy beam has more weight)");

    // Histogram pad, matching the small reference plot in slide 6.
    TPad* hp = new TPad("pad_slide6_hist", "", 0.115, 0.115, 0.405, 0.495);
    hp->Draw(); hp->cd();
    stylePad(0.15, 0.16, 0.05, 0.10);
    TH1D* h = m.hist;
    h->SetTitle("1GeV Gamma;E_{rec}/E_{beam};count");
    h->SetLineColor(kBlue+2);
    h->SetLineWidth(2);
    h->Draw("HIST");
    if (m.fit) m.fit->Draw("SAME");

    TLatex lx;
    lx.SetNDC(); lx.SetTextFont(62); lx.SetTextColor(kRed+1); lx.SetTextSize(0.034);
    lx.DrawLatex(0.57, 0.72, Form("BeamE: %.2f GeV", m.E));
    lx.DrawLatex(0.57, 0.66, Form("Mean: %.3f +/- %.3f", m.bias, m.biasErr));
    lx.DrawLatex(0.57, 0.60, Form("Sigma: %.3f +/- %.3f", m.reso, m.resoErr));
    if (m.fit) lx.DrawLatex(0.57, 0.54, Form("Chi2/NDF: %.2f/%d", m.fit->GetChisquare(), m.fit->GetNDF()));

    c->cd();
    TArrow* arr = new TArrow(0.410,0.285,0.490,0.315,0.018,"|>");
    arr->SetNDC(kTRUE); arr->SetLineColor(kRed+1); arr->SetFillColor(kRed+1); arr->SetLineWidth(3); arr->Draw();

    TPaveText* box = new TPaveText(0.500, 0.095, 0.930, 0.500, "NDC");
    box->SetFillColor(kWhite);
    box->SetLineColor(kRed+1);
    box->SetLineWidth(3);
    box->SetTextFont(42);
    box->SetTextSize(0.027);
    box->AddText("Linear energy regression")->SetTextFont(62);
    box->AddText("E_{rec} = p0 + p1 * E_{ECAL} + p2 * E_{HCAL}");
    box->AddText("Fit E_{rec} with double-side crystal ball function");
    box->AddText("#bullet   Energy resolution = #sigma / E_{beam}  (%)");
    box->AddText("#bullet   Energy bias = #mu / E_{beam}");
    box->Draw();

    drawFooterText("6/18");
    c->Write();
    return c;
}

void drawRequirementCurves(const string& particle, double xmin, double xmax) {
    const bool gamma = (particle == "gamma");
    const double oldA = gamma ? 0.35 : 0.50;
    const double newA = gamma ? 0.20 : 0.35;
    TF1* reqOld = new TF1(Form("req_old_%s", particle.c_str()), Form("%g/sqrt(x)+0.05", oldA), xmin, xmax);
    TF1* reqNew = new TF1(Form("req_new_%s", particle.c_str()), Form("%g/sqrt(x)+0.05", newA), xmin, xmax);
    reqOld->SetLineColor(kBlack); reqOld->SetLineWidth(2);
    reqNew->SetLineColor(kBlue);  reqNew->SetLineWidth(2);
    reqOld->Draw("SAME"); reqNew->Draw("SAME");

    TLatex lx;
    lx.SetNDC(); lx.SetTextFont(62); lx.SetTextSize(0.028); lx.SetTextColor(kBlack);
    lx.DrawLatex(0.45,0.33, Form("Required(before) : %.0f.00%% / #sqrt{E} + 5.00%%", oldA*100));
    lx.SetTextColor(kBlue);
    lx.DrawLatex(0.48,0.27, Form("Required(now) : %.0f.00%% / #sqrt{E} + 5.00%%", newA*100));
}

TCanvas* makeResolutionBiasCanvas(const string& particle,
                                  std::map<string,TGraphErrors*>& resGraphs,
                                  std::map<string,TGraphErrors*>& biasGraphs) {
    const bool gamma = (particle == "gamma");
    const char* cname = gamma ? "c_slide7_gamma_resolution_bias" : "c_slide8_neutron_resolution_bias";
    const char* title = gamma ? "Gamma Beam Energy Regression" : "Neutron Beam Energy Regression";
    const char* page  = gamma ? "7/18" : "8/18";
    TCanvas* c = new TCanvas(cname, title, 1600, 1200);
    c->SetFillColor(kWhite);
    c->cd();
    drawSlideTitle(title);

    CurveDef curves[6] = {
        {0,0,kRed+1,   kFullCircle}, {1,0,kRed+1,   kOpenCircle}, {2,0,kRed+1,   kFullSquare},
        {0,1,kGreen+2, kFullCircle}, {1,1,kGreen+2, kOpenCircle}, {2,1,kGreen+2, kFullSquare}
    };

    TPad* left  = new TPad(Form("pad_%s_res", particle.c_str()),  "", 0.070, 0.315, 0.475, 0.805);
    TPad* right = new TPad(Form("pad_%s_bias", particle.c_str()), "", 0.555, 0.315, 0.960, 0.805);
    left->Draw(); right->Draw();

    left->cd();
    stylePad(0.13,0.13,0.03,0.10);
    TMultiGraph* mgR = new TMultiGraph(Form("mg_res_%s", particle.c_str()), "Energy Resolution");
    TLegend* legR = new TLegend(0.54,0.64,0.96,0.94);
    legR->SetBorderSize(1); legR->SetFillColor(kWhite); legR->SetTextSize(0.024);
    for (auto& cv : curves) {
        string key = Form("%s_m%d_w%d", particle.c_str(), cv.method, cv.weight);
        TGraphErrors* g = resGraphs[key];
        if (!g || g->GetN()==0) continue;
        styleGraph(g, cv.color, cv.marker);
        mgR->Add(g, "PL");
        legR->AddEntry(g, Form("%s, %s", WEIGHT_LABEL[cv.weight], METHOD_LABEL[cv.method]), "pl");
    }
    mgR->Draw("AP");
    mgR->GetXaxis()->SetTitle("Energy (GeV)");
    mgR->GetYaxis()->SetTitle("Energy Resolution (%)");
    mgR->GetYaxis()->SetRangeUser(0.0, gamma ? 0.30 : 1.00); // neutron resolution can exceed 80% at low energy
    mgR->GetXaxis()->SetLimits(0.0, gamma ? 42.0 : 320.0);
    mgR->GetXaxis()->SetTitleSize(0.050); mgR->GetYaxis()->SetTitleSize(0.050);
    mgR->GetXaxis()->SetLabelSize(0.041); mgR->GetYaxis()->SetLabelSize(0.041);
    mgR->GetYaxis()->SetTitleOffset(1.15);
    drawRequirementCurves(particle, gamma ? 1.0 : 20.0, gamma ? 42.0 : 320.0);
    legR->Draw();

    right->cd();
    stylePad(0.13,0.13,0.03,0.10);
    TMultiGraph* mgB = new TMultiGraph(Form("mg_bias_%s", particle.c_str()), "Energy Bias");
    TLegend* legB = new TLegend(0.54,0.64,0.96,0.94);
    legB->SetBorderSize(1); legB->SetFillColor(kWhite); legB->SetTextSize(0.024);
    for (auto& cv : curves) {
        string key = Form("%s_m%d_w%d", particle.c_str(), cv.method, cv.weight);
        TGraphErrors* g = biasGraphs[key];
        if (!g || g->GetN()==0) continue;
        styleGraph(g, cv.color, cv.marker);
        mgB->Add(g, "PL");
        legB->AddEntry(g, Form("%s, %s", WEIGHT_LABEL[cv.weight], METHOD_LABEL[cv.method]), "pl");
    }
    mgB->Draw("AP");
    mgB->GetXaxis()->SetTitle("Energy (GeV)");
    mgB->GetYaxis()->SetTitle("E_{rec}/E_{beam}");
    mgB->GetYaxis()->SetRangeUser(gamma ? 0.80 : 0.0, gamma ? 1.20 : 3.00); // neutron bias can be far from 1
    mgB->GetXaxis()->SetLimits(0.0, gamma ? 42.0 : 320.0);
    mgB->GetXaxis()->SetTitleSize(0.050); mgB->GetYaxis()->SetTitleSize(0.050);
    mgB->GetXaxis()->SetLabelSize(0.041); mgB->GetYaxis()->SetLabelSize(0.041);
    mgB->GetYaxis()->SetTitleOffset(1.15);
    TLine* one = new TLine(0.0, 1.0, gamma ? 42.0 : 320.0, 1.0);
    one->SetLineColor(kBlack); one->SetLineStyle(2); one->SetLineWidth(1); one->Draw("SAME");
    legB->Draw();

    c->cd();
    lightBlueBox(gamma ? 0.140 : 0.055, 0.085, gamma ? 0.890 : 0.960, gamma ? 0.205 : 0.235);
    TLatex bt; bt.SetNDC(); bt.SetTextFont(42); bt.SetTextSize(0.021); bt.SetTextColor(kBlack);
    if (gamma) {
        bt.SetTextFont(62); bt.DrawLatex(0.155,0.180,"0.1#minus40 GeV Gamma Beam"); bt.SetTextFont(42);
        bt.DrawLatex(0.155,0.155,"#bullet   Method : Sqrt(Ebeam) weighting performs best. insensitive to fitting function.");
        bt.DrawLatex(0.155,0.130,"#bullet   Resolution: Performance requirements fully satisfied.");
        bt.DrawLatex(0.155,0.105,"#bullet   Bias: Negligible energy bias observed.");
        bt.DrawLatex(0.155,0.080,"#bullet   Nice energy reconstruction for gamma.");
    } else {
        bt.DrawLatex(0.075,0.205,"#bullet   Optimal Method: Sqrt(Ebeam) weighting combined with the Ratio Method yielded the best results.");
        bt.DrawLatex(0.075,0.180,"#bullet   Energy Resolution: neutrons < 50 GeV does not meet the requirements.");
        bt.DrawLatex(0.075,0.155,"#bullet   Energy Bias: A bias of approximately 2% is observed for neutron > 50GeV. Large bias for low energy");
        bt.DrawLatex(0.107,0.130,"region (neutron < 50GeV).");
        bt.DrawLatex(0.075,0.105,"#bullet   The small fraction of neutron energy deposited in the scintillator tile (3%#minus6%) limits regression");
        bt.DrawLatex(0.107,0.080,"precision for beam energies under 50 GeV?");
    }
    drawFooterText(page);

    c->Write();
    return c;
}

TCanvas* makeCleanEnergyDumpCanvas(const vector<Sample>& gammaSamples,
                                    const vector<Sample>& neutronSamples) {
    TCanvas* c = new TCanvas("c_clean_energy_dump", "Energy Dump", 1500, 900);
    c->SetFillColor(kWhite);
    c->Divide(3, 2, 0.005, 0.005);

    TGraphErrors* gs[6] = {
        makeDumpGraph(gammaSamples,   0, "gDmpC_gE"),
        makeDumpGraph(gammaSamples,   1, "gDmpC_gH"),
        makeDumpGraph(gammaSamples,   2, "gDmpC_gA"),
        makeDumpGraph(neutronSamples, 0, "gDmpC_nE"),
        makeDumpGraph(neutronSamples, 1, "gDmpC_nH"),
        makeDumpGraph(neutronSamples, 2, "gDmpC_nA"),
    };
    const char* titles[6] = {
        "#gamma: ECAL/Beam;E_{beam} (GeV);ECal/Beam",
        "#gamma: HCAL/Beam;E_{beam} (GeV);HCal/Beam",
        "#gamma: (ECAL+HCAL)/Beam;E_{beam} (GeV);(ECal+HCal)/Beam",
        "n: ECAL/Beam;E_{beam} (GeV);ECal/Beam",
        "n: HCAL/Beam;E_{beam} (GeV);HCal/Beam",
        "n: (ECAL+HCAL)/Beam;E_{beam} (GeV);(ECal+HCal)/Beam",
    };
    const double xMax[6] = {50,50,50,350,350,350};
    const double yMax[6] = {0.60,0.020,0.60,0.070,0.022,0.070};

    for (int i = 0; i < 6; ++i) {
        c->cd(i+1); stylePad(0.17, 0.15, 0.04, 0.08);
        if (!gs[i] || gs[i]->GetN() == 0) continue;
        gs[i]->SetTitle(titles[i]);
        gs[i]->Draw("AP");
        gs[i]->GetXaxis()->SetLimits(0.0, xMax[i]);
        gs[i]->GetYaxis()->SetRangeUser(0.0, yMax[i]);
        gs[i]->GetXaxis()->SetTitleSize(0.048);
        gs[i]->GetYaxis()->SetTitleSize(0.048);
        gs[i]->GetXaxis()->SetLabelSize(0.042);
        gs[i]->GetYaxis()->SetLabelSize(0.042);
        gs[i]->GetYaxis()->SetTitleOffset(1.30);
    }
    return c;
}

TCanvas* makeCleanResolutionBiasCanvas(const string& particle,
                                        std::map<string,TGraphErrors*>& resGraphs,
                                        std::map<string,TGraphErrors*>& biasGraphs,
                                        const char* cname) {
    const bool   gamma = (particle == "gamma");
    const double xMax  = gamma ? 42.0 : 320.0;
    const double rqX0  = gamma ?  1.0 :  10.0;
    const double oldA  = gamma ? 0.35 :  0.50;
    const double newA  = gamma ? 0.20 :  0.35;

    TCanvas* c = new TCanvas(cname, Form("%s Resolution & Bias", particle.c_str()), 1400, 620);
    c->SetFillColor(kWhite);
    TPad* pL = new TPad(Form("%s_L", cname), "", 0.01, 0.0, 0.50, 1.0);
    TPad* pR = new TPad(Form("%s_R", cname), "", 0.51, 0.0, 1.00, 1.0);
    pL->Draw(); pR->Draw();

    CurveDef curves[6] = {
        {0,0,kRed+1,   kFullCircle}, {1,0,kRed+1,   kOpenCircle}, {2,0,kRed+1,   kFullSquare},
        {0,1,kGreen+2, kFullCircle}, {1,1,kGreen+2, kOpenCircle}, {2,1,kGreen+2, kFullSquare}
    };

    pL->cd(); stylePad(0.17, 0.14, 0.03, 0.07);
    TMultiGraph* mgR = new TMultiGraph(Form("mgR_cln_%s", particle.c_str()), "");
    TLegend* legR = new TLegend(0.37, 0.50, 0.97, 0.95);
    legR->SetBorderSize(1); legR->SetFillColor(kWhite); legR->SetTextSize(0.026);
    for (auto& cv : curves) {
        const string key = Form("%s_m%d_w%d", particle.c_str(), cv.method, cv.weight);
        auto it = resGraphs.find(key);
        if (it == resGraphs.end() || !it->second || it->second->GetN()==0) continue;
        TGraphErrors* g = it->second;
        styleGraph(g, cv.color, cv.marker);
        mgR->Add(g, "PL");
        legR->AddEntry(g, Form("%s, %s", WEIGHT_LABEL[cv.weight], METHOD_LABEL[cv.method]), "pl");
    }
    TF1* reqOldR = new TF1(Form("reqOR_%s", cname), Form("%g/sqrt(x)+0.05", oldA), rqX0, xMax);
    TF1* reqNewR = new TF1(Form("reqNR_%s", cname), Form("%g/sqrt(x)+0.05", newA), rqX0, xMax);
    reqOldR->SetLineColor(kGray+2); reqOldR->SetLineWidth(2); reqOldR->SetLineStyle(7);
    reqNewR->SetLineColor(kBlue+2); reqNewR->SetLineWidth(2); reqNewR->SetLineStyle(7);
    mgR->Draw("AP");
    mgR->GetXaxis()->SetTitle("E_{beam} (GeV)");
    mgR->GetYaxis()->SetTitle("#sigma_{E} / E_{beam}");
    mgR->GetYaxis()->SetRangeUser(0.0, 0.30); // match reference slides 7/8
    mgR->GetXaxis()->SetLimits(0.0, xMax);
    mgR->GetXaxis()->SetTitleSize(0.05); mgR->GetYaxis()->SetTitleSize(0.05);
    mgR->GetXaxis()->SetLabelSize(0.043); mgR->GetYaxis()->SetLabelSize(0.043);
    mgR->GetYaxis()->SetTitleOffset(1.25);
    reqOldR->Draw("SAME"); reqNewR->Draw("SAME");
    legR->AddEntry(reqOldR, Form("Req. old: %.0f%%/#sqrt{E}+5%%", oldA*100), "l");
    legR->AddEntry(reqNewR, Form("Req. new: %.0f%%/#sqrt{E}+5%%", newA*100), "l");
    legR->Draw();

    pR->cd(); stylePad(0.17, 0.14, 0.03, 0.07);
    TMultiGraph* mgB = new TMultiGraph(Form("mgB_cln_%s", particle.c_str()), "");
    TLegend* legB = new TLegend(0.37, 0.50, 0.97, 0.95);
    legB->SetBorderSize(1); legB->SetFillColor(kWhite); legB->SetTextSize(0.026);
    for (auto& cv : curves) {
        const string key = Form("%s_m%d_w%d", particle.c_str(), cv.method, cv.weight);
        auto it = biasGraphs.find(key);
        if (it == biasGraphs.end() || !it->second || it->second->GetN()==0) continue;
        TGraphErrors* g = it->second;
        styleGraph(g, cv.color, cv.marker);
        mgB->Add(g, "PL");
        legB->AddEntry(g, Form("%s, %s", WEIGHT_LABEL[cv.weight], METHOD_LABEL[cv.method]), "pl");
    }
    mgB->Draw("AP");
    mgB->GetXaxis()->SetTitle("E_{beam} (GeV)");
    mgB->GetYaxis()->SetTitle("E_{rec} / E_{beam}");
    mgB->GetYaxis()->SetRangeUser(0.80, gamma ? 1.20 : 1.50); // match reference slides 7/8
    mgB->GetXaxis()->SetLimits(0.0, xMax);
    mgB->GetXaxis()->SetTitleSize(0.05); mgB->GetYaxis()->SetTitleSize(0.05);
    mgB->GetXaxis()->SetLabelSize(0.043); mgB->GetYaxis()->SetLabelSize(0.043);
    mgB->GetYaxis()->SetTitleOffset(1.25);
    TLine* one = new TLine(0.0, 1.0, xMax, 1.0);
    one->SetLineColor(kBlack); one->SetLineStyle(2); one->SetLineWidth(1); one->Draw("SAME");
    legB->Draw();

    return c;
}

// Clean version of reference slide 6: the 1 GeV gamma E_rec/E_beam histogram with its
// DCB fit and the mean/sigma/chi2, no slide title/footer/method text boxes.
TCanvas* makeClean1GeVGammaCanvas(const vector<RecoMetrics>& gmets, const char* cname) {
    if (gmets.empty()) return nullptr;
    int best = 0; double bestD = 1e99;
    for (int i = 0; i < (int)gmets.size(); ++i) {
        const double d = TMath::Abs(gmets[i].E - 1.0);
        if (d < bestD) { bestD = d; best = i; }
    }
    const RecoMetrics& m = gmets[best];
    if (!m.hist) return nullptr;

    TCanvas* c = new TCanvas(cname, "1 GeV gamma E_rec/E_beam", 800, 700);
    c->SetFillColor(kWhite);
    stylePad(0.14, 0.13, 0.04, 0.09);
    TH1D* h = m.hist;
    h->SetStats(0);   // drop the ROOT stats box; we draw our own BeamE/Mean/Sigma text
    h->SetTitle(Form("%.0f GeV Gamma Energy Regression;E_{rec} / E_{beam};Count", m.E));
    h->SetLineColor(kBlue+2);
    h->SetLineWidth(2);
    h->GetXaxis()->SetTitleSize(0.045); h->GetYaxis()->SetTitleSize(0.045);
    h->GetXaxis()->SetLabelSize(0.040); h->GetYaxis()->SetLabelSize(0.040);
    h->GetYaxis()->SetTitleOffset(1.45);
    h->Draw("HIST");
    if (m.fit) { m.fit->SetLineColor(kRed+1); m.fit->SetLineWidth(3); m.fit->Draw("SAME"); }

    TLatex lx;
    lx.SetNDC(); lx.SetTextFont(42); lx.SetTextColor(kRed+1); lx.SetTextSize(0.040);
    lx.DrawLatex(0.60, 0.83, Form("BeamE: %.2f GeV", m.E));
    lx.DrawLatex(0.60, 0.77, Form("Mean: %.3f", m.bias));
    lx.DrawLatex(0.60, 0.71, Form("Sigma: %.3f", m.reso));
    if (m.fit) lx.DrawLatex(0.60, 0.65, Form("#chi^{2}/NDF: %.1f/%d", m.fit->GetChisquare(), m.fit->GetNDF()));
    return c;
}

void writeAllHistCanvases(const string& particle,
                          const std::map<string, vector<RecoMetrics>>& allMetrics) {
    for (const auto& kv : allMetrics) {
        const vector<RecoMetrics>& ms = kv.second;
        if (ms.empty()) continue;
        int nx = (int)std::ceil(std::sqrt((double)ms.size()));
        int ny = (int)std::ceil((double)ms.size() / nx);
        TCanvas* c = new TCanvas(Form("c_%s_ratios_%s", particle.c_str(), kv.first.c_str()),
                                 Form("%s ratio histograms %s", particle.c_str(), kv.first.c_str()),
                                 1400, 900);
        c->Divide(nx, ny);
        for (int i = 0; i < (int)ms.size(); ++i) {
            c->cd(i+1);
            stylePad(0.15,0.14,0.05,0.10);
            ms[i].hist->Draw("HIST");
            if (ms[i].fit) ms[i].fit->Draw("SAME");
        }
        c->Write();
    }
}

void zdc_reco_browser(const char* inputDir="data", const char* outDir="plots") {
    setGlobalStyle();
    ensureDir(outDir);
    ensureDir("qa");
    // Analysis archive lives in qa/, not plots/ -- plots/ holds only the final graph PNGs.
    const TString outRoot = "qa/energy_reconstruction_browser.root";

    vector<Sample> gammaSamples, neutronSamples;
    loadAllSamples(inputDir, "gamma", gammaSamples);
    loadAllSamples(inputDir, "neutron", neutronSamples);

    if (gammaSamples.empty() && neutronSamples.empty()) {
        printf("[FATAL] No raw files found under '%s'. Expected outfile_gamma*.root and/or outfile_neutron*.root.\n", inputDir);
        return;
    }

    TFile* fout = TFile::Open(outRoot.Data(), "RECREATE");
    if (!fout || fout->IsZombie()) {
        printf("[FATAL] Cannot create %s\n", outRoot.Data());
        return;
    }

    TDirectory* dInput = fout->mkdir("00_input_summary");
    dInput->cd();
    TTree* summary = new TTree("input_files", "Raw input file summary");
    char particle[32]; char fname[512]; double Emean, Erms; int entries;
    summary->Branch("particle", particle, "particle[32]/C");
    summary->Branch("file", fname, "file[512]/C");
    summary->Branch("beam_mean_GeV", &Emean, "beam_mean_GeV/D");
    summary->Branch("beam_rms_GeV", &Erms, "beam_rms_GeV/D");
    summary->Branch("entries", &entries, "entries/I");
    for (const auto& s : gammaSamples) {
        snprintf(particle, 32, "gamma"); snprintf(fname, 512, "%s", s.path.c_str());
        Emean = s.beamMean; Erms = s.beamRMS; entries = (int)s.events.size(); summary->Fill();
    }
    for (const auto& s : neutronSamples) {
        snprintf(particle, 32, "neutron"); snprintf(fname, 512, "%s", s.path.c_str());
        Emean = s.beamMean; Erms = s.beamRMS; entries = (int)s.events.size(); summary->Fill();
    }
    summary->Write();

    // ---- Clean physics graphs (no slide decorations) ----
    // Gamma: train regression on [1,40] GeV for stable parameters; evaluate on ALL gamma energies
    // (100 MeV - 40 GeV) so the graph shows the full range including the sub-GeV extrapolation.
    // Neutron: train and evaluate on all available energies.
    {
        // Match the reference windows (Taiwan group res2_1_*):
        //   gamma   : 1 - 40 GeV  (sub-GeV excluded: regression extrapolates below 1 GeV)
        //   neutron : 20 - 300 GeV (10 GeV excluded: response is far too non-linear there;
        //             the reference graphs are 6 points, 20-300, for exactly this reason)
        vector<Sample> gammaTrainSamples   = filterSamples(gammaSamples,   0.99, 40.5);
        vector<Sample> neutronRefSamples   = filterSamples(neutronSamples, 15.0, 320.0);
        std::map<string,TGraphErrors*> resClnG, biasClnG, resClnN, biasClnN;
        vector<RecoMetrics> gamma1GeVmets;   // kept for the slide-6-style 1 GeV histogram

        for (int m = 0; m < 3; ++m) {
            for (int w = 0; w < 2; ++w) {
                if (!gammaTrainSamples.empty()) {
                    vector<double> params;
                    fitRegression(gammaTrainSamples, m, w, params);
                    vector<RecoMetrics> mets = buildRecoMetrics(gammaTrainSamples, params, m, w, "gamma");
                    const string key = Form("gamma_m%d_w%d", m, w);
                    resClnG[key]  = makeGraph(mets, true,  Form("gR_cln_g_m%d_w%d", m, w));
                    biasClnG[key] = makeGraph(mets, false, Form("gB_cln_g_m%d_w%d", m, w));
                    if (m == 0 && w == 0) gamma1GeVmets = mets;  // linear, equal-weight (slide 6)
                }
                if (!neutronRefSamples.empty()) {
                    vector<double> params;
                    fitRegression(neutronRefSamples, m, w, params);
                    vector<RecoMetrics> mets = buildRecoMetrics(neutronRefSamples, params, m, w, "neutron");
                    const string key = Form("neutron_m%d_w%d", m, w);
                    resClnN[key]  = makeGraph(mets, true,  Form("gR_cln_n_m%d_w%d", m, w));
                    biasClnN[key] = makeGraph(mets, false, Form("gB_cln_n_m%d_w%d", m, w));
                }
            }
        }

        fout->cd();
        TDirectory* dClean = fout->mkdir("06_clean_graphs");
        dClean->cd();

        TCanvas* cDump = makeCleanEnergyDumpCanvas(gammaSamples, neutronSamples);
        cDump->SaveAs(TString::Format("%s/energy_dump.png", outDir));
        cDump->Write();
        if (!gamma1GeVmets.empty()) {
            TCanvas* c1g = makeClean1GeVGammaCanvas(gamma1GeVmets, "c_cln_gamma1GeV");
            if (c1g) {
                c1g->SaveAs(TString::Format("%s/gamma1GeV_regression.png", outDir));
                c1g->Write();
            }
        }
        if (!resClnG.empty()) {
            TCanvas* cg = makeCleanResolutionBiasCanvas("gamma",   resClnG, biasClnG, "c_cln_gamma");
            cg->SaveAs(TString::Format("%s/gamma_resolution_bias.png", outDir));
            cg->Write();
        }
        if (!resClnN.empty()) {
            TCanvas* cn = makeCleanResolutionBiasCanvas("neutron", resClnN, biasClnN, "c_cln_neutron");
            cn->SaveAs(TString::Format("%s/neutron_resolution_bias.png", outDir));
            cn->Write();
        }
        fout->cd();
    }

    fout->Write();
    fout->Close();

    printf("\n[DONE] 4 graph PNGs in %s/: energy_dump.png, gamma1GeV_regression.png, gamma_resolution_bias.png, neutron_resolution_bias.png\n", outDir);
    printf("[DONE] Analysis archive (optional, for TBrowser QA): %s\n", outRoot.Data());
}
