// plot_slide7_8_resolution.C
// Reproduces slides 7-8: Energy resolution + bias vs beam energy
// 6 curves per particle (m0-2 x w0-1), plus requirement TF1 lines
// Output: plots/slide7_gamma_resolution.png, slide8_neutron_resolution.png
//         + written to plots/energy_reconstruction.root
// Usage: root -b -q scripts/plot_slide7_8_resolution.C

struct Curve {
    int    m, w;
    int    color;
    int    marker;
    const char* label;
};

TGraphErrors* getGraph(const char* path, const char* name) {
    TFile *f = TFile::Open(path);
    if (!f || f->IsZombie()) { printf("[ERROR] Cannot open %s\n", path); return nullptr; }
    TGraphErrors *g = (TGraphErrors*)f->Get(name);
    if (!g) { printf("[ERROR] %s not in %s\n", name, path); f->Close(); return nullptr; }
    // Clone to survive file close
    TGraphErrors *gc = (TGraphErrors*)g->Clone(TString::Format("%s_%s", name, gSystem->BaseName(path)));
    f->Close();
    return gc;
}

void drawPanel(
    TMultiGraph *mg, TLegend *leg,
    const char* title, const char* xtitle, const char* ytitle,
    double ymin, double ymax,
    TF1 *fOld, TF1 *fNew,
    const char* reqOldLabel, const char* reqNewLabel
) {
    gPad->SetLeftMargin(0.14); gPad->SetBottomMargin(0.13); gPad->SetRightMargin(0.04);
    gPad->SetTicks(1,1);
    mg->SetTitle(title);
    mg->Draw("AP");
    mg->GetXaxis()->SetTitle(xtitle);
    mg->GetYaxis()->SetTitle(ytitle);
    mg->GetYaxis()->SetRangeUser(ymin, ymax);
    mg->GetXaxis()->SetTitleSize(0.05);
    mg->GetYaxis()->SetTitleSize(0.05);
    mg->GetXaxis()->SetLabelSize(0.045);
    mg->GetYaxis()->SetLabelSize(0.045);
    mg->GetYaxis()->SetTitleOffset(1.2);

    if (fOld) {
        fOld->SetLineColor(kGray+1); fOld->SetLineWidth(2); fOld->SetLineStyle(7);
        fOld->Draw("same");
        leg->AddEntry(fOld, reqOldLabel, "l");
    }
    if (fNew) {
        fNew->SetLineColor(kBlack); fNew->SetLineWidth(2); fNew->SetLineStyle(2);
        fNew->Draw("same");
        leg->AddEntry(fNew, reqNewLabel, "l");
    }
    leg->SetBorderSize(1); leg->SetFillStyle(1001); leg->SetTextSize(0.038);
    leg->Draw();
    gPad->RedrawAxis();
}

void makePlot(
    const char* base, const char* particle, const char* dir,
    Curve *curves, int ncurves,
    double xmin, double xmax,
    double rmin, double rmax,
    double bmin, double bmax,
    double reqOldP0, double reqNewP0,
    const char* reqOldLabel, const char* reqNewLabel,
    const char* outPng, const char* canvName
) {
    TMultiGraph *mgRes  = new TMultiGraph();
    TMultiGraph *mgBias = new TMultiGraph();
    TLegend *legRes  = new TLegend(0.42, 0.62, 0.95, 0.95);
    TLegend *legBias = new TLegend(0.42, 0.62, 0.95, 0.95);

    for (int i = 0; i < ncurves; i++) {
        Curve &cv = curves[i];
        TString path = TString::Format("%s/%s/res2_1_m%d_w%d_%s.root", base, dir, cv.m, cv.w, particle);
        TGraphErrors *grRes  = getGraph(path.Data(), "gReso_dcb");
        TGraphErrors *grBias = getGraph(path.Data(), "gBias_dcb");
        if (!grRes || !grBias) continue;

        for (auto *g : {grRes, grBias}) {
            g->SetMarkerStyle(cv.marker);
            g->SetMarkerColor(cv.color);
            g->SetLineColor(cv.color);
            g->SetMarkerSize(1.2);
            g->SetLineWidth(2);
        }
        mgRes->Add(grRes,  "PL");
        mgBias->Add(grBias, "PL");
        legRes->AddEntry(grRes,  cv.label, "p");
        legBias->AddEntry(grBias, cv.label, "p");
    }

    // Requirement TF1s (formula: p0/sqrt(x) + p1/x + p2, p1=0)
    TF1 *fResOld = new TF1("fResOld", "[0]/sqrt(x)+[2]", xmin, xmax);
    fResOld->SetParameters(reqOldP0, 0, 0.05);
    TF1 *fResNew = new TF1("fResNew", "[0]/sqrt(x)+[2]", xmin, xmax);
    fResNew->SetParameters(reqNewP0, 0, 0.05);

    TString title = TString::Format("%s Energy Resolution vs Beam Energy", particle);
    TString titleB= TString::Format("%s Energy Bias vs Beam Energy", particle);

    TCanvas *c = new TCanvas(canvName, title, 1400, 600);
    c->Divide(2, 1, 0.005, 0.005);

    c->cd(1);
    drawPanel(mgRes, legRes, title.Data(), "Beam Energy (GeV)", "#sigma(E_{rec}/E_{beam})",
              rmin, rmax, fResOld, fResNew, reqOldLabel, reqNewLabel);

    c->cd(2);
    // Bias: no requirement lines
    gPad->SetLeftMargin(0.14); gPad->SetBottomMargin(0.13); gPad->SetRightMargin(0.04);
    gPad->SetTicks(1,1);
    mgBias->SetTitle(titleB.Data());
    mgBias->Draw("AP");
    mgBias->GetXaxis()->SetTitle("Beam Energy (GeV)");
    mgBias->GetYaxis()->SetTitle("#mu(E_{rec}/E_{beam})");
    mgBias->GetYaxis()->SetRangeUser(bmin, bmax);
    mgBias->GetXaxis()->SetTitleSize(0.05);
    mgBias->GetYaxis()->SetTitleSize(0.05);
    mgBias->GetXaxis()->SetLabelSize(0.045);
    mgBias->GetYaxis()->SetLabelSize(0.045);
    mgBias->GetYaxis()->SetTitleOffset(1.2);
    legBias->SetBorderSize(1); legBias->SetFillStyle(1001); legBias->SetTextSize(0.038);
    legBias->Draw();
    gPad->RedrawAxis();

    const char* base_c = base;
    c->SaveAs(TString::Format("%s/plots/%s", base_c, outPng));

    TFile *out = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base_c), "UPDATE");
    out->cd(); c->Write(canvName);
    out->Close();

    printf("[OK] %s saved\n", outPng);
}

void plot_slide7_8_resolution() {
    gStyle->SetOptStat(0);
    gStyle->SetPadGridX(1);
    gStyle->SetPadGridY(1);

    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    // 6 method curves: m0-2 x w0-1 (w2 not in PDF)
    Curve curves[] = {
        {0, 0, kRed+1,    kFullCircle,       "Equal-Weighted, Linear Fun."},
        {1, 0, kBlue+1,   kFullSquare,       "Equal-Weighted, Ratio Fun."},
        {2, 0, kBlack,    kFullTriangleUp,   "Equal-Weighted, Quadratic Fun."},
        {0, 1, kGreen+2,  kOpenCircle,       "#sqrt{E}-Weighted, Linear Fun."},
        {1, 1, kMagenta+1,kOpenSquare,       "#sqrt{E}-Weighted, Ratio Fun."},
        {2, 1, kOrange+2, kOpenTriangleUp,   "#sqrt{E}-Weighted, Quadratic Fun."},
    };
    int ncurves = sizeof(curves)/sizeof(curves[0]);

    // Slide 7: Gamma (0.7-40 GeV)
    // Gamma requirement: old=35%/sqrtE+5%, new=20%/sqrtE+5%
    makePlot(base, "gamma", "20260421_gamma_LYSO_diffAngle",
             curves, ncurves,
             0.5, 50,        // xmin, xmax
             0.0, 0.30,      // resolution y range
             0.90, 1.10,     // bias y range
             0.35, 0.20,     // req old p0, req new p0
             "Old: 35%/#sqrt{E}+5%", "New: 20%/#sqrt{E}+5%",
             "slide7_gamma_resolution.png", "slide7_gamma_resolution");

    // Slide 8: Neutron (20-300 GeV)
    // Neutron requirement: old=50%/sqrtE+5%, new=35%/sqrtE+5%
    makePlot(base, "neutron", "20260324_neutron_LYSO_diffAngle",
             curves, ncurves,
             10, 350,        // xmin, xmax
             0.0, 0.45,      // resolution y range
             0.90, 1.10,     // bias y range
             0.50, 0.35,     // req old p0, req new p0
             "Old: 50%/#sqrt{E}+5%", "New: 35%/#sqrt{E}+5%",
             "slide8_neutron_resolution.png", "slide8_neutron_resolution");

    printf("[DONE] Slides 7-8 complete\n");
}
