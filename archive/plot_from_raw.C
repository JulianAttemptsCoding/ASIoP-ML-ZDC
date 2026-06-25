// plot_from_raw.C — full pipeline from raw outfile_*.root to all 4 energy-reconstruction plots
// Slides 5-8: energy dump, E_rec distribution, gamma resolution/bias, neutron resolution/bias
// Raw data: PODIO TTree "events" with hit arrays + MCParticles
// Regression methods: m0=Linear, m1=Ratio, m2=Quadratic  |  w0=equal, w1=sqrt(E_beam)
// DCB fit implemented from scratch as TF1

#include <vector>
#include <string>
#include <cmath>

// ─── DCB function ─────────────────────────────────────────────────────────────
// p[0]=mu  p[1]=sigma  p[2]=alphaL  p[3]=nL  p[4]=alphaR  p[5]=nR  p[6]=norm
Double_t dcbFunc(Double_t *x, Double_t *p) {
    Double_t t  = (x[0] - p[0]) / p[1];
    Double_t aL = TMath::Abs(p[2]), nL = p[3];
    Double_t aR = TMath::Abs(p[4]), nR = p[5];
    Double_t val;
    if (t < -aL) {
        Double_t A = TMath::Power(nL/aL, nL) * TMath::Exp(-aL*aL/2.0);
        Double_t B = nL/aL - aL;
        Double_t arg = B - t;
        if (arg <= 0) return 0;
        val = A * TMath::Power(arg, -nL);
    } else if (t > aR) {
        Double_t A = TMath::Power(nR/aR, nR) * TMath::Exp(-aR*aR/2.0);
        Double_t B = nR/aR - aR;
        Double_t arg = B + t;
        if (arg <= 0) return 0;
        val = A * TMath::Power(arg, -nR);
    } else {
        val = TMath::Exp(-t*t/2.0);
    }
    return p[6] * val;
}

// ─── Per-file cache (read each file exactly once) ─────────────────────────────
struct FileData { std::vector<double> ecal, hcal, beam; };
std::map<std::string, FileData> gFileCache;

// ─── Load raw file into cache using TLeaf::GetValue() ────────────────────────
// No SetBranchStatus — dot-notation leaves need their parent branch active.
bool loadFile(const char* path) {
    std::string key(path);
    if (gFileCache.count(key)) return true;

    TFile *f = TFile::Open(path);
    if (!f || f->IsZombie()) { printf("[WARN] Cannot open %s\n", path); return false; }
    TTree *t = (TTree*)f->Get("events");
    if (!t) { f->Close(); return false; }

    TLeaf *lEcal = t->GetLeaf("EcalFarForwardZDCHits.energy");
    TLeaf *lHcal = t->GetLeaf("HcalFarForwardZDCHits.energy");
    TLeaf *lPx   = t->GetLeaf("MCParticles.momentum.x");
    TLeaf *lPy   = t->GetLeaf("MCParticles.momentum.y");
    TLeaf *lPz   = t->GetLeaf("MCParticles.momentum.z");
    TLeaf *lMass = t->GetLeaf("MCParticles.mass");
    TLeaf *lStat = t->GetLeaf("MCParticles.generatorStatus");

    if (!lEcal||!lHcal||!lPx||!lPy||!lPz||!lMass||!lStat) {
        printf("[WARN] Missing leaves in %s\n", path); f->Close(); return false;
    }

    FileData &d = gFileCache[key];
    for (Long64_t iev = 0, nev = t->GetEntries(); iev < nev; iev++) {
        t->GetEntry(iev);

        double sumEcal = 0;
        for (int ih = 0, n = lEcal->GetLen(); ih < n; ih++) sumEcal += lEcal->GetValue(ih);

        double sumHcal = 0;
        for (int ih = 0, n = lHcal->GetLen(); ih < n; ih++) sumHcal += lHcal->GetValue(ih);

        double eBeam = 0;
        for (int ip = 0, n = lStat->GetLen(); ip < n; ip++) {
            if ((int)lStat->GetValue(ip) == 1) {
                double px = lPx->GetValue(ip), py = lPy->GetValue(ip);
                double pz = lPz->GetValue(ip), m  = lMass->GetValue(ip);
                eBeam = sqrt(px*px + py*py + pz*pz + m*m);
                break;
            }
        }
        if (eBeam < 1e-6) continue;
        d.ecal.push_back(sumEcal);
        d.hcal.push_back(sumHcal);
        d.beam.push_back(eBeam);
    }
    f->Close();
    return true;
}

// Append cached data to caller's vectors (no re-read)
bool readFile(const char* path,
              std::vector<double>& vEcal, std::vector<double>& vHcal,
              std::vector<double>& vBeam) {
    if (!loadFile(path)) return false;
    const FileData &d = gFileCache[std::string(path)];
    vEcal.insert(vEcal.end(), d.ecal.begin(), d.ecal.end());
    vHcal.insert(vHcal.end(), d.hcal.begin(), d.hcal.end());
    vBeam.insert(vBeam.end(), d.beam.begin(), d.beam.end());
    return true;
}

// ─── Regression fit via TLinearFitter ────────────────────────────────────────
// method: 0=linear, 1=ratio, 2=quadratic   wmode: 0=equal, 1=sqrt(E_beam)
// m0: p0 + p1*E_ECAL + p2*E_HCAL                                    (3 params)
// m1: p0 + p1*E_ECAL + p2*E_HCAL + p3*E_HCAL/(E_ECAL+E_HCAL)       (4 params)
// m2: p0 + p1*E_ECAL + p2*E_HCAL + p3*E²_ECAL + p4*E²_HCAL         (5 params)
void fitRegression(const std::vector<double>& vEcal,
                   const std::vector<double>& vHcal,
                   const std::vector<double>& vBeam,
                   int method, int wmode,
                   std::vector<double>& params) {
    int n = vEcal.size();
    int nfeat;
    if (method == 0) nfeat = 2;
    else if (method == 1) nfeat = 3;
    else nfeat = 4;
    TLinearFitter lf(nfeat, "1 ++ x[0] ++ x[1]");
    if (method == 1) lf.SetFormula("1 ++ x[0] ++ x[1] ++ x[2]");
    if (method == 2) lf.SetFormula("1 ++ x[0] ++ x[1] ++ x[2] ++ x[3]");

    int nAdded = 0;
    for (int i = 0; i < n; i++) {
        double ec = vEcal[i], hc = vHcal[i], eb = vBeam[i];
        double tot = ec + hc;
        // Skip events with negligible visible energy — they drag p0 up
        if (tot < eb * 0.001) continue;
        // err = 1/sqrt(w): w0→w=1→err=1, w1→w=sqrt(eb)→err=eb^{-1/4}
        double err = (wmode == 0) ? 1.0 : 1.0 / sqrt(sqrt(eb));

        double xx[4];
        if (method == 0) { xx[0]=ec; xx[1]=hc; }
        else if (method == 1) {
            double hr = (tot > 1e-9) ? hc/tot : 0.0;
            xx[0]=ec; xx[1]=hc; xx[2]=hr;
        }
        else { xx[0]=ec; xx[1]=hc; xx[2]=ec*ec; xx[3]=hc*hc; }
        lf.AddPoint(xx, eb, err);
        nAdded++;
    }
    int k = (method == 0) ? 3 : (method == 1) ? 4 : 5;
    if (nAdded < 10) { params.assign(k, 0); return; }
    lf.Eval();
    params.resize(k);
    for (int j = 0; j < k; j++) params[j] = lf.GetParameter(j);
}

// ─── Apply reconstruction ────────────────────────────────────────────────────
double applyRec(double ec, double hc, int method, const std::vector<double>& p) {
    double tot = ec + hc;
    if (method == 0) return p[0] + p[1]*ec + p[2]*hc;
    if (method == 1) {
        double hr = (tot > 1e-9) ? hc/tot : 0.0;
        return p[0] + p[1]*ec + p[2]*hc + p[3]*hr;
    }
    // m2: p0 + p1*ec + p2*hc + p3*ec² + p4*hc²
    return p[0] + p[1]*ec + p[2]*hc + p[3]*ec*ec + p[4]*hc*hc;
}

// ─── DCB fit ──────────────────────────────────────────────────────────────────
// Robust initialization: start from histogram mode region, clipped RMS.
// Constrains mu to stay near the main peak to avoid p0-spike traps.
bool fitDCB(TH1D *h, double &mu, double &sig, double &muE, double &sigE) {
    mu = sig = muE = sigE = 0;
    if (!h || h->GetEntries() < 20) return false;
    double xlo = h->GetXaxis()->GetXmin();
    double xhi = h->GetXaxis()->GetXmax();

    // Find peak bin (mode)
    int modeBin = h->GetMaximumBin();
    double hmode = h->GetXaxis()->GetBinCenter(modeBin);

    // Two-pass mean/RMS within ±0.8 of mode (numerical precision: use (x-hmode))
    double sumW=0, sumWd=0, sumWd2=0;
    for (int b=1; b<=h->GetNbinsX(); b++) {
        double x = h->GetXaxis()->GetBinCenter(b);
        double c = h->GetBinContent(b);
        if (c > 0 && TMath::Abs(x - hmode) <= 0.8) {
            double d = x - hmode;
            sumW += c; sumWd += c*d; sumWd2 += c*d*d;
        }
    }
    if (sumW < 5) return false;
    double locMean = hmode + sumWd/sumW;
    double locVar  = sumWd2/sumW - (sumWd/sumW)*(sumWd/sumW);
    double locRMS  = (locVar > 1e-8) ? sqrt(locVar) : h->GetRMS()*0.5;
    if (locRMS < 0.01) locRMS = TMath::Max(0.02, h->GetRMS()*0.5);

    // One round of 2-sigma clipping (two-pass, stable)
    {
        double lo2 = locMean - 2*locRMS, hi2 = locMean + 2*locRMS;
        double sw=0, swd=0, swd2=0;
        for (int b=1; b<=h->GetNbinsX(); b++) {
            double x = h->GetXaxis()->GetBinCenter(b);
            double c = h->GetBinContent(b);
            if (c > 0 && x >= lo2 && x <= hi2) {
                double d = x - locMean;
                sw+=c; swd+=c*d; swd2+=c*d*d;
            }
        }
        if (sw >= 5) {
            locMean += swd/sw;
            double cv = swd2/sw - (swd/sw)*(swd/sw);
            if (cv > 1e-8) locRMS = sqrt(cv);
        }
    }
    if (locRMS < 0.01) locRMS = TMath::Max(0.02, h->GetRMS()*0.5);

    double norm = h->GetMaximum() * locRMS * sqrt(2*TMath::Pi());
    if (norm < 1e-9) norm = 1.0;

    // Fit range: locMean ± 3.5*locRMS, clamped
    double flo = TMath::Max(xlo, locMean - 3.5*locRMS);
    double fhi = TMath::Min(xhi, locMean + 3.5*locRMS);
    if (fhi - flo < 0.05) { flo = locMean-0.3; fhi = locMean+0.3; }
    flo = TMath::Max(xlo, flo); fhi = TMath::Min(xhi, fhi);

    static int dcbCount = 0;
    TString fname = TString::Format("dcb_tmp_%d", dcbCount++);
    TF1 *f = new TF1(fname, dcbFunc, flo, fhi, 7);
    // Use full histogram RMS to set sigma window — anchors to true distribution width
    double hRMS = h->GetRMS();
    if (hRMS < 0.01) hRMS = locRMS;
    double sigInit = TMath::Max(locRMS, hRMS * 0.7);
    f->SetParameters(locMean, sigInit, 1.2, 5.0, 1.5, 5.0, norm);
    // Constrain mu within ±1σ of local mean (prevents drift to p0 spike)
    f->SetParLimits(0, locMean - locRMS, locMean + locRMS);
    // Constrain sigma to [50%, 200%] of histogram RMS — prevents spurious narrow core
    f->SetParLimits(1, TMath::Max(0.01, hRMS*0.5), TMath::Min(hRMS*2.0, 1.5));
    f->SetParLimits(2, 0.3, 8.0);
    f->SetParLimits(3, 1.1, 50.0);
    f->SetParLimits(4, 0.3, 8.0);
    f->SetParLimits(5, 1.1, 50.0);
    f->SetParLimits(6, 0.0, 1e12);

    TFitResultPtr r = h->Fit(f, "QNRS");
    bool ok = (r.Get() && r->Status() < 8);

    mu   = f->GetParameter(0); muE  = f->GetParError(0);
    sig  = TMath::Abs(f->GetParameter(1)); sigE = f->GetParError(1);

    // Sanity: mu must stay near local mean; sigma must be physically meaningful
    bool drifted = (TMath::Abs(mu - locMean) > 1.5*locRMS);
    double sigMin = TMath::Max(0.01, h->GetRMS()*0.3);  // global floor from histogram
    if (TMath::IsNaN(mu) || TMath::IsNaN(sig) ||
        mu < flo || mu > fhi || sig < sigMin || sig > 1.5 || drifted) {
        mu = locMean; sig = locRMS;
        muE = locRMS / sqrt(sumW); sigE = muE;
        ok = true;
    }

    delete f;
    return ok;
}

// ─── Gamma file list ──────────────────────────────────────────────────────────
struct GFile { const char* name; double energy; };
GFile gammaFiles[] = {
    // energy >= 0.7 GeV matching original analysis
    {"outfile_gamma700MeV.root",  0.7},
    {"outfile_gamma900MeV.root",  0.9},
    {"outfile_gamma1GeV.root",    1.0},
    {"outfile_gamma3GeV.root",    3.0},
    {"outfile_gamma5GeV.root",    5.0},
    {"outfile_gamma7GeV.root",    7.0},
    {"outfile_gamma9GeV.root",    9.0},
    {"outfile_gamma10GeV.root",  10.0},
    {"outfile_gamma20GeV.root",  20.0},
    {"outfile_gamma40GeV.root",  40.0},
};
int nGammaFiles = sizeof(gammaFiles)/sizeof(gammaFiles[0]);

// neutronFiles: all 7 for training; evaluation plot skips neutron1 (10 GeV)
struct NFile { const char* name; bool plotIt; };
NFile neutronFiles[] = {
    {"outfile_neutron1.root", false},  // 10 GeV: in training, not in eval plot
    {"outfile_neutron2.root", true},
    {"outfile_neutron3.root", true},
    {"outfile_neutron4.root", true},
    {"outfile_neutron5.root", true},
    {"outfile_neutron6.root", true},
    {"outfile_neutron7.root", true},
};
int nNeutronFiles = sizeof(neutronFiles)/sizeof(neutronFiles[0]);

// ─── Styling helpers ─────────────────────────────────────────────────────────
// Colors match reference: red=equal-weighted, green=sqrt-weighted
struct Curve { int m, w, color, marker; const char* label; };
Curve curves[] = {
    {0,0, kRed+1,   kFullCircle,     "Equal-Weighted,  Linear Fun."},
    {1,0, kRed+1,   kOpenCircle,     "Equal-Weighted,  Ratio Fun."},
    {2,0, kRed+1,   kOpenTriangleUp, "Equal-Weighted,  Quadratic Fun."},
    {0,1, kGreen+2, kFullCircle,     "#sqrt{E}_{beam}-weighted,  Linear Fun."},
    {1,1, kGreen+2, kOpenCircle,     "#sqrt{E}_{beam}-weighted,  Ratio Fun."},
    {2,1, kGreen+2, kFullSquare,     "#sqrt{E}_{beam}-weighted,  Quadratic Fun."},
};
int nCurves = sizeof(curves)/sizeof(curves[0]);

void styleGraph(TGraphErrors *g, int color, int marker) {
    g->SetMarkerStyle(marker); g->SetMarkerColor(color);
    g->SetLineColor(color);    g->SetMarkerSize(1.3);
    g->SetLineWidth(2);
}

// ─── MAIN ────────────────────────────────────────────────────────────────────
void plot_from_raw() {
    gStyle->SetOptStat(0);
    gStyle->SetTitleFontSize(0.065);
    gStyle->SetPadGridX(1); gStyle->SetPadGridY(1);

    const char* base  = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";
    const char* gddir = "20260421_gamma_LYSO_diffAngle";
    const char* nddir = "20260324_neutron_LYSO_diffAngle";
    TString outRoot   = TString::Format("%s/plots/energy_reconstruction.root", base);

    // Delete old output file to start clean
    gSystem->Unlink(outRoot);

    // ══════════════════════════════════════════════════════════════════════
    // SLIDE 5 — Energy dump: TProfile of E_sub/E_beam vs E_beam
    // ══════════════════════════════════════════════════════════════════════
    printf("\n[Slide 5] Building energy dump profiles...\n");

    // Profile objects: gamma (3) + neutron (3)
    TProfile *gEcal = new TProfile("gEcal","Gamma:  E_{ECAL} / E_{beam}",         50, 0, 50);
    TProfile *gHcal = new TProfile("gHcal","Gamma:  E_{HCAL} / E_{beam}",         50, 0, 50);
    TProfile *gAll  = new TProfile("gAll", "Gamma:  (E_{ECAL}+E_{HCAL}) / E_{beam}", 50, 0, 50);
    TProfile *nEcal = new TProfile("nEcal","Neutron:  E_{ECAL} / E_{beam}",        350, 0, 350);
    TProfile *nHcal = new TProfile("nHcal","Neutron:  E_{HCAL} / E_{beam}",        350, 0, 350);
    TProfile *nAll  = new TProfile("nAll", "Neutron:  (E_{ECAL}+E_{HCAL}) / E_{beam}", 350, 0, 350);

    // Fill gamma profiles
    for (int fi = 0; fi < nGammaFiles; fi++) {
        std::vector<double> vE, vH, vB;
        TString path = TString::Format("%s/%s/%s", base, gddir, gammaFiles[fi].name);
        if (!readFile(path, vE, vH, vB)) continue;
        double eNom = gammaFiles[fi].energy;
        for (int i = 0; i < (int)vB.size(); i++) {
            gEcal->Fill(eNom, vE[i]/vB[i]);
            gHcal->Fill(eNom, vH[i]/vB[i]);
            gAll ->Fill(eNom, (vE[i]+vH[i])/vB[i]);
        }
        printf("  gamma %.1f GeV: %d events\n", eNom, (int)vB.size());
    }

    // Fill neutron profiles
    for (int fi = 0; fi < nNeutronFiles; fi++) {
        std::vector<double> vE, vH, vB;
        TString path = TString::Format("%s/%s/%s", base, nddir, neutronFiles[fi].name);
        if (!readFile(path, vE, vH, vB)) continue;
        double eNom = vB[0];  // use first event's beam energy as nominal
        for (int i = 0; i < (int)vB.size(); i++) {
            nEcal->Fill(eNom, vE[i]/vB[i]);
            nHcal->Fill(eNom, vH[i]/vB[i]);
            nAll ->Fill(eNom, (vE[i]+vH[i])/vB[i]);
        }
        printf("  neutron %.0f GeV: %d events\n", eNom, (int)vB.size());
    }

    // Style profiles
    auto styleProf = [](TProfile *p, int col, const char* yt, double ylo, double yhi) {
        p->SetMarkerStyle(kOpenCircle); p->SetMarkerColor(col); p->SetLineColor(col);
        p->SetMarkerSize(1.2);
        p->GetXaxis()->SetTitle("Beam Energy (GeV)");
        p->GetYaxis()->SetTitle(yt);
        p->GetYaxis()->SetRangeUser(ylo, yhi);
        p->GetXaxis()->SetTitleSize(0.055); p->GetYaxis()->SetTitleSize(0.055);
        p->GetXaxis()->SetLabelSize(0.050); p->GetYaxis()->SetLabelSize(0.050);
        p->GetXaxis()->SetTitleOffset(1.05); p->GetYaxis()->SetTitleOffset(1.30);
        p->GetYaxis()->SetMaxDigits(3);
    };
    styleProf(gEcal, kBlue+1, "E_{ECAL} / E_{beam}",             0, 0.65);
    styleProf(gHcal, kBlue+1, "E_{HCAL} / E_{beam}",             0, 0.021);
    styleProf(gAll,  kBlue+1, "(E_{ECAL}+E_{HCAL}) / E_{beam}",  0, 0.65);
    styleProf(nEcal, kRed+1,  "E_{ECAL} / E_{beam}",             0, 0.060);
    styleProf(nHcal, kRed+1,  "E_{HCAL} / E_{beam}",             0, 0.021);
    styleProf(nAll,  kRed+1,  "(E_{ECAL}+E_{HCAL}) / E_{beam}",  0, 0.060);

    auto setupPad = []() {
        gPad->SetLeftMargin(0.17); gPad->SetBottomMargin(0.15);
        gPad->SetRightMargin(0.03); gPad->SetTopMargin(0.11);
        gPad->SetTicks(1,1);
    };

    TCanvas *c5 = new TCanvas("slide5_energy_dump","Energy Dump",2700,1500);
    c5->Divide(3, 2, 0.003, 0.003);
    c5->cd(1); setupPad(); gEcal->Draw("P E1");
    c5->cd(2); setupPad(); gHcal->Draw("P E1");
    c5->cd(3); setupPad(); gAll ->Draw("P E1");
    c5->cd(4); setupPad(); nEcal->Draw("P E1");
    c5->cd(5); setupPad(); nHcal->Draw("P E1");
    c5->cd(6); setupPad(); nAll ->Draw("P E1");
    c5->SaveAs(TString::Format("%s/plots/slide5_energy_dump.png", base));
    TFile *fout = TFile::Open(outRoot, "UPDATE");
    fout->cd(); c5->Write("slide5_energy_dump"); fout->Close();
    printf("[OK] slide5_energy_dump.png\n");
    delete c5;

    // ══════════════════════════════════════════════════════════════════════
    // SLIDE 6 — E_rec/E_beam distribution: 1 GeV gamma, m0 w1
    // ══════════════════════════════════════════════════════════════════════
    printf("\n[Slide 6] Building E_rec distribution (1 GeV gamma, m0 w1)...\n");

    // Need regression params trained on ALL gamma data
    std::vector<double> allGecal, allGhcal, allGbeam;
    for (int fi = 0; fi < nGammaFiles; fi++) {
        TString path = TString::Format("%s/%s/%s", base, gddir, gammaFiles[fi].name);
        readFile(path, allGecal, allGhcal, allGbeam);
    }

    std::vector<double> p6;
    fitRegression(allGecal, allGhcal, allGbeam, 0, 1, p6);  // m0 w1
    printf("  m0 w1 params: p0=%.4f p1=%.4f p2=%.4f\n", p6[0], p6[1], p6[2]);

    // 1 GeV gamma events
    std::vector<double> v1E, v1H, v1B;
    readFile(TString::Format("%s/%s/outfile_gamma1GeV.root", base, gddir), v1E, v1H, v1B);
    printf("  1 GeV gamma events: %d\n", (int)v1B.size());

    TH1D *h6 = new TH1D("h6","E_{rec}/E_{beam}  at  1 GeV #gamma",50, 0, 2);
    for (int i = 0; i < (int)v1B.size(); i++) {
        double erec = applyRec(v1E[i], v1H[i], 0, p6);
        if (v1B[i] > 0) h6->Fill(erec / v1B[i]);
    }
    h6->SetLineColor(kBlack);
    h6->GetXaxis()->SetTitle("E_{rec} / E_{beam}");
    h6->GetYaxis()->SetTitle("Counts");
    h6->GetXaxis()->SetRangeUser(0, 2);
    h6->GetXaxis()->SetTitleSize(0.052); h6->GetYaxis()->SetTitleSize(0.052);
    h6->GetXaxis()->SetLabelSize(0.048); h6->GetYaxis()->SetLabelSize(0.048);
    h6->GetXaxis()->SetTitleOffset(1.05); h6->GetYaxis()->SetTitleOffset(1.15);

    // DCB fit
    TF1 *fDCB6 = new TF1("dcb6", dcbFunc, 0.0, 2.0, 7);
    double m6=h6->GetMean(), s6=h6->GetRMS();
    fDCB6->SetParameters(m6, s6, 1.0, 5.0, 2.0, 5.0, h6->GetMaximum()*s6*2.5);
    fDCB6->SetParLimits(1, 1e-4, 2.0); fDCB6->SetParLimits(2, 0.1, 5.0);
    fDCB6->SetParLimits(3, 1.1, 50.0); fDCB6->SetParLimits(4, 0.1, 5.0);
    fDCB6->SetParLimits(5, 1.1, 50.0); fDCB6->SetParLimits(6, 0, 1e9);
    h6->Fit(fDCB6, "QNR");
    fDCB6->SetLineColor(kRed); fDCB6->SetLineWidth(3);

    double mu6=fDCB6->GetParameter(0), muE6=fDCB6->GetParError(0);
    double sg6=fDCB6->GetParameter(1), sgE6=fDCB6->GetParError(1);
    double chi6=fDCB6->GetChisquare(), ndf6=fDCB6->GetNDF();
    printf("  DCB: mu=%.3f+/-%.3f  sigma=%.3f+/-%.3f  chi2/ndf=%.2f/%d\n",
           mu6, muE6, sg6, sgE6, chi6, (int)ndf6);

    TCanvas *c6 = new TCanvas("slide6_erec_dist","E_rec Distribution",1600,1200);
    c6->SetLeftMargin(0.14); c6->SetBottomMargin(0.13);
    c6->SetRightMargin(0.05); c6->SetTopMargin(0.08);
    h6->Draw("HIST");
    fDCB6->Draw("same");

    // Stats box
    TPaveText *box6 = new TPaveText(0.64, 0.56, 0.99, 0.91, "NDC");
    box6->SetFillColor(0); box6->SetBorderSize(1);
    box6->SetTextAlign(12); box6->SetTextSize(0.042); box6->SetTextFont(42);
    box6->AddText(TString::Format("BeamE: 1.00 GeV"));
    box6->AddText(TString::Format("Mean : %.2f +/- %.2f", mu6, muE6));
    box6->AddText(TString::Format("Sigma: %.2f +/- %.2f", sg6, sgE6));
    box6->AddText(TString::Format("Chi2/NDF : %.2f/%.0f", chi6, ndf6));
    box6->Draw();

    c6->SaveAs(TString::Format("%s/plots/slide6_erec_dist.png", base));
    fout = TFile::Open(outRoot, "UPDATE");
    fout->cd(); c6->Write("slide6_erec_dist"); fout->Close();
    printf("[OK] slide6_erec_dist.png\n");
    delete c6;

    // ══════════════════════════════════════════════════════════════════════
    // SLIDES 7 & 8 — Resolution & bias vs beam energy
    // ══════════════════════════════════════════════════════════════════════
    struct Particle {
        const char* name; const char* dir;
        double reqOldP0, reqNewP0;
        const char* reqOldLbl, *reqNewLbl;
        double xlo, xhi, rhi, blo, bhi;
        const char* outPng, *canvName;
        int slideNum;
    };

    Particle particles[] = {
        {"gamma",   gddir, 0.35, 0.20,
         "Old req: 35%/#sqrt{E}+5%", "New req: 20%/#sqrt{E}+5%",
         0.5, 50, 0.30, 0.80, 1.20,
         "slide7_gamma_resolution.png", "slide7_gamma_resolution", 7},
        {"neutron", nddir, 0.50, 0.35,
         "Old req: 50%/#sqrt{E}+5%", "New req: 35%/#sqrt{E}+5%",
         10, 350, 0.45, 0.80, 2.70,
         "slide8_neutron_resolution.png", "slide8_neutron_resolution", 8},
    };

    for (int ipart = 0; ipart < 2; ipart++) {
        Particle &P = particles[ipart];
        printf("\n[Slide %d] Processing %s...\n", P.slideNum, P.name);

        // Determine which files and nominal energies to use
        std::vector<TString> fPaths;
        std::vector<double>  eNoms;

        bool isGamma = (ipart == 0);
        std::vector<bool> plotFlags;  // which files appear in the eval plot
        if (isGamma) {
            for (int fi = 0; fi < nGammaFiles; fi++) {
                fPaths.push_back(TString::Format("%s/%s/%s", base, P.dir, gammaFiles[fi].name));
                eNoms.push_back(gammaFiles[fi].energy);
                plotFlags.push_back(gammaFiles[fi].energy >= 1.0);  // skip 0.7, 0.9 GeV in plot
            }
        } else {
            for (int fi = 0; fi < nNeutronFiles; fi++) {
                TString path = TString::Format("%s/%s/%s", base, P.dir, neutronFiles[fi].name);
                fPaths.push_back(path);
                // read first event to get nominal energy
                std::vector<double> tmpE, tmpH, tmpB;
                readFile(path, tmpE, tmpH, tmpB);
                eNoms.push_back(tmpB.empty() ? 0 : tmpB[0]);
                plotFlags.push_back(neutronFiles[fi].plotIt);
            }
        }

        // Load ALL events for regression training
        std::vector<double> allEc, allHc, allBe;
        for (auto &fp : fPaths) readFile(fp, allEc, allHc, allBe);
        printf("  Total training events: %d\n", (int)allBe.size());

        // For each of 6 method/weighting combos
        TMultiGraph *mgRes  = new TMultiGraph(TString::Format("mgR_%s",  P.name), "");
        TMultiGraph *mgBias = new TMultiGraph(TString::Format("mgB_%s",  P.name), "");
        struct LEntry { TObject *obj; const char* lbl; const char* opt; };
        std::vector<LEntry> legE;

        for (int ic = 0; ic < nCurves; ic++) {
            Curve &cv = curves[ic];

            // Fit regression on all training data
            std::vector<double> params;
            fitRegression(allEc, allHc, allBe, cv.m, cv.w, params);
            printf("  m%d w%d params:", cv.m, cv.w);
            for (auto &pp : params) printf(" %.3f", pp);
            printf("\n");

            // For each energy point, compute E_rec/E_beam distribution, fit DCB
            std::vector<double> xpts, yRes, yBias, expts, eRes, eBias;

            for (int fi = 0; fi < (int)fPaths.size(); fi++) {
                if (eNoms[fi] < 1e-6) continue;
                if (!plotFlags[fi]) continue;  // training-only file, skip in plot
                std::vector<double> vE, vH, vB;
                readFile(fPaths[fi], vE, vH, vB);
                if (vB.empty()) continue;

                double eNom = eNoms[fi];
                // Wide range [0,5] avoids overflow when bias is large (e.g. neutron low E)
                TH1D *htmp = new TH1D(
                    TString::Format("htmp_%s_m%d_w%d_%d", P.name, cv.m, cv.w, fi),
                    "", 100, 0, 5);
                for (int i = 0; i < (int)vB.size(); i++) {
                    if (vB[i] <= 0) continue;
                    if ((vE[i]+vH[i]) < vB[i]*0.001) continue;  // skip near-zero deposits
                    double erec = applyRec(vE[i], vH[i], cv.m, params);
                    htmp->Fill(erec / vB[i]);
                }

                double mu, sig, muE, sigE;
                bool ok = fitDCB(htmp, mu, sig, muE, sigE);
                delete htmp;

                if (!ok || sig < 0.005 || mu < 0.01 || mu > 4.9) {
                    printf("  [WARN] DCB failed: %s m%d w%d E=%.0f  mu=%.3f sig=%.4f\n",
                           P.name, cv.m, cv.w, eNom, mu, sig);
                    continue;
                }
                // Cap errors at 50% of central value to avoid huge error bars
                if (sigE > 0.5*sig || sigE <= 0) sigE = 0.1*sig;
                if (muE  > 0.05    || muE  <= 0) muE  = 0.01;
                xpts.push_back(eNom); expts.push_back(0);
                yRes.push_back(sig);  eRes.push_back(sigE);
                yBias.push_back(mu);  eBias.push_back(muE);
                printf("  m%d w%d  E=%6.1f GeV  mu=%.3f  sig=%.3f\n",
                       cv.m, cv.w, eNom, mu, sig);
            }

            if (xpts.empty()) continue;
            int np = xpts.size();
            TGraphErrors *grRes  = new TGraphErrors(np, xpts.data(), yRes.data(),
                                                    expts.data(), eRes.data());
            TGraphErrors *grBias = new TGraphErrors(np, xpts.data(), yBias.data(),
                                                    expts.data(), eBias.data());
            grRes->SetName(TString::Format("gRes_%s_m%d_w%d",  P.name, cv.m, cv.w));
            grBias->SetName(TString::Format("gBias_%s_m%d_w%d", P.name, cv.m, cv.w));
            styleGraph(grRes,  cv.color, cv.marker);
            styleGraph(grBias, cv.color, cv.marker);
            mgRes ->Add(grRes,  "PL");
            mgBias->Add(grBias, "PL");
            legE.push_back({grRes, cv.label, "p"});
        }

        // Requirement TF1s
        TF1 *fOld = new TF1(TString::Format("fOld_%s",P.name),
                             "[0]/sqrt(x)+[2]", P.xlo, P.xhi);
        fOld->SetParameters(P.reqOldP0, 0, 0.05);
        fOld->SetLineColor(kGray+2); fOld->SetLineWidth(3); fOld->SetLineStyle(7);

        TF1 *fNew = new TF1(TString::Format("fNew_%s",P.name),
                             "[0]/sqrt(x)+[2]", P.xlo, P.xhi);
        fNew->SetParameters(P.reqNewP0, 0, 0.05);
        fNew->SetLineColor(kOrange+2); fNew->SetLineWidth(3); fNew->SetLineStyle(2);
        legE.push_back({fOld, P.reqOldLbl, "l"});
        legE.push_back({fNew, P.reqNewLbl, "l"});

        // Build canvas: 3 pads [resolution | bias | legend]
        TString capLabel = P.name; capLabel[0] = toupper(capLabel[0]);
        TCanvas *c78 = new TCanvas(P.canvName, P.canvName, 3800, 1200);
        TPad *pRes  = new TPad("pRes",  "", 0.00, 0.0, 0.41, 1.0); pRes->Draw();
        TPad *pBias = new TPad("pBias", "", 0.42, 0.0, 0.77, 1.0); pBias->Draw();
        TPad *pLeg  = new TPad("pLeg",  "", 0.78, 0.0, 1.00, 1.0); pLeg->Draw();

        auto styleDataPad = []() {
            gPad->SetLeftMargin(0.14); gPad->SetBottomMargin(0.14);
            gPad->SetRightMargin(0.03); gPad->SetTopMargin(0.10);
            gPad->SetTicks(1,1); gPad->SetGrid(1,1);
        };
        auto styleAxes = [](TMultiGraph *mg, const char* xt, const char* yt,
                             double ylo, double yhi) {
            mg->Draw("AP");
            mg->GetXaxis()->SetTitle(xt);
            mg->GetYaxis()->SetTitle(yt);
            mg->GetYaxis()->SetRangeUser(ylo, yhi);
            mg->GetXaxis()->SetTitleSize(0.054); mg->GetYaxis()->SetTitleSize(0.054);
            mg->GetXaxis()->SetLabelSize(0.048); mg->GetYaxis()->SetLabelSize(0.048);
            mg->GetXaxis()->SetTitleOffset(1.05); mg->GetYaxis()->SetTitleOffset(1.20);
            mg->GetYaxis()->SetMaxDigits(3);
        };

        pRes->cd(); styleDataPad();
        mgRes->SetTitle(TString::Format("%s: Energy Resolution", capLabel.Data()));
        styleAxes(mgRes, "Energy (GeV)", "Energy Resolution (%)", 0.0, P.rhi);
        fOld->Draw("same"); fNew->Draw("same");
        gPad->RedrawAxis();

        pBias->cd(); styleDataPad();
        mgBias->SetTitle(TString::Format("%s: Energy Bias", capLabel.Data()));
        styleAxes(mgBias, "Energy (GeV)", "E_{Rec}/E_{Beam}", P.blo, P.bhi);
        gPad->RedrawAxis();

        pLeg->cd();
        pLeg->SetFillColor(0); pLeg->SetBorderSize(0);
        TLegend *leg = new TLegend(0.03, 0.05, 0.97, 0.95);
        leg->SetBorderSize(1); leg->SetFillStyle(1001);
        leg->SetTextSize(0.063); leg->SetTextFont(42);
        leg->SetHeader(TString::Format("  %s Methods", capLabel.Data()), "C");
        for (auto &e : legE) leg->AddEntry(e.obj, e.lbl, e.opt);
        leg->Draw();

        c78->SaveAs(TString::Format("%s/plots/%s", base, P.outPng));
        fout = TFile::Open(outRoot, "UPDATE");
        fout->cd(); c78->Write(P.canvName); fout->Close();
        printf("[OK] %s\n", P.outPng);
        delete c78;
    }

    printf("\n[DONE] All plots written to plots/ and energy_reconstruction.root\n");
}
