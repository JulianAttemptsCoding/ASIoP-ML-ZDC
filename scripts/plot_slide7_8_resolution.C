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
    TGraphErrors *gc = (TGraphErrors*)g->Clone(TString::Format("%s_%s", name, gSystem->BaseName(path)));
    f->Close();
    return gc;
}

void stylePanel(TMultiGraph *mg, const char* title, const char* xtitle, const char* ytitle,
                double ymin, double ymax) {
    gPad->SetLeftMargin(0.13); gPad->SetBottomMargin(0.13);
    gPad->SetRightMargin(0.03); gPad->SetTopMargin(0.09);
    gPad->SetTicks(1,1);
    mg->SetTitle(title);
    mg->Draw("AP");
    mg->GetXaxis()->SetTitle(xtitle);
    mg->GetYaxis()->SetTitle(ytitle);
    mg->GetYaxis()->SetRangeUser(ymin, ymax);
    mg->GetXaxis()->SetTitleSize(0.052);
    mg->GetYaxis()->SetTitleSize(0.052);
    mg->GetXaxis()->SetLabelSize(0.046);
    mg->GetYaxis()->SetLabelSize(0.046);
    mg->GetXaxis()->SetTitleOffset(1.05);
    mg->GetYaxis()->SetTitleOffset(1.15);
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

    // Legend position: left side of upper region, wide enough for long labels
    // Resolution legend has 8 entries (6 data + 2 req lines)
    TLegend *legRes  = new TLegend(0.35, 0.52, 0.97, 0.91);
    TLegend *legBias = new TLegend(0.35, 0.60, 0.97, 0.91);

    for (int i = 0; i < ncurves; i++) {
        Curve &cv = curves[i];
        TString path = TString::Format("%s/%s/res2_1_m%d_w%d_%s.root", base, dir, cv.m, cv.w, particle);
        TGraphErrors *grRes  = getGraph(path.Data(), "gReso_dcb");
        TGraphErrors *grBias = getGraph(path.Data(), "gBias_dcb");
        if (!grRes || !grBias) continue;

        for (TGraphErrors *g : {grRes, grBias}) {
            g->SetMarkerStyle(cv.marker);
            g->SetMarkerColor(cv.color);
            g->SetLineColor(cv.color);
            g->SetMarkerSize(1.4);
            g->SetLineWidth(2);
        }
        mgRes->Add(grRes,  "PL");
        mgBias->Add(grBias, "PL");
        legRes->AddEntry(grRes,  cv.label, "p");
        legBias->AddEntry(grBias, cv.label, "p");
    }

    // Requirement lines: p0/sqrt(x) + p2
    TF1 *fResOld = new TF1("fResOld", "[0]/sqrt(x)+[2]", xmin, xmax);
    fResOld->SetParameters(reqOldP0, 0, 0.05);
    fResOld->SetLineColor(kGray+2); fResOld->SetLineWidth(3); fResOld->SetLineStyle(7);

    TF1 *fResNew = new TF1("fResNew", "[0]/sqrt(x)+[2]", xmin, xmax);
    fResNew->SetParameters(reqNewP0, 0, 0.05);
    fResNew->SetLineColor(kOrange+3); fResNew->SetLineWidth(3); fResNew->SetLineStyle(2);

    legRes->AddEntry(fResOld, reqOldLabel, "l");
    legRes->AddEntry(fResNew, reqNewLabel, "l");

    TString partLabel = particle;
    partLabel[0] = toupper(partLabel[0]);
    TString titleRes  = TString::Format("%s: Energy Resolution vs Beam Energy", partLabel.Data());
    TString titleBias = TString::Format("%s: Energy Bias vs Beam Energy",       partLabel.Data());

    TCanvas *c = new TCanvas(canvName, titleRes, 2800, 1200);
    c->Divide(2, 1, 0.003, 0.003);

    c->cd(1);
    stylePanel(mgRes, titleRes.Data(), "Beam Energy (GeV)", "#sigma (E_{rec} / E_{beam})", rmin, rmax);
    fResOld->Draw("same");
    fResNew->Draw("same");
    legRes->SetBorderSize(1); legRes->SetFillStyle(1001);
    legRes->SetTextSize(0.034); legRes->SetTextFont(42);
    legRes->Draw();
    gPad->RedrawAxis();

    c->cd(2);
    stylePanel(mgBias, titleBias.Data(), "Beam Energy (GeV)", "#mu (E_{rec} / E_{beam})", bmin, bmax);
    legBias->SetBorderSize(1); legBias->SetFillStyle(1001);
    legBias->SetTextSize(0.034); legBias->SetTextFont(42);
    legBias->Draw();
    gPad->RedrawAxis();

    c->SaveAs(TString::Format("%s/plots/%s", base, outPng));

    TFile *out = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base), "UPDATE");
    out->cd(); c->Write(canvName);
    out->Close();

    printf("[OK] %s saved\n", outPng);
    delete c;
}

void plot_slide7_8_resolution() {
    gStyle->SetOptStat(0);
    gStyle->SetPadGridX(1);
    gStyle->SetPadGridY(1);

    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    // 6 method curves: m0-2 x w0-1 (w2 not in PDF)
    Curve curves[] = {
        {0, 0, kRed+1,     kFullCircle,     "Equal-Weighted,  Linear Fun."},
        {1, 0, kBlue+1,    kFullSquare,     "Equal-Weighted,  Ratio Fun."},
        {2, 0, kBlack,     kFullTriangleUp, "Equal-Weighted,  Quadratic Fun."},
        {0, 1, kGreen+2,   kOpenCircle,     "#sqrt{E}-Weighted,  Linear Fun."},
        {1, 1, kMagenta+1, kOpenSquare,     "#sqrt{E}-Weighted,  Ratio Fun."},
        {2, 1, kOrange+2,  kOpenTriangleUp, "#sqrt{E}-Weighted,  Quadratic Fun."},
    };
    int ncurves = sizeof(curves)/sizeof(curves[0]);

    // Slide 7: Gamma (0.7-40 GeV); req: old=35%/sqrtE+5%, new=20%/sqrtE+5%
    makePlot(base, "gamma", "20260421_gamma_LYSO_diffAngle",
             curves, ncurves,
             0.5, 50,
             0.0, 0.30,
             0.90, 1.10,
             0.35, 0.20,
             "Old req: 35%/#sqrt{E} + 5%", "New req: 20%/#sqrt{E} + 5%",
             "slide7_gamma_resolution.png", "slide7_gamma_resolution");

    // Slide 8: Neutron (20-300 GeV); req: old=50%/sqrtE+5%, new=35%/sqrtE+5%
    makePlot(base, "neutron", "20260324_neutron_LYSO_diffAngle",
             curves, ncurves,
             10, 350,
             0.0, 0.45,
             0.90, 1.10,
             0.50, 0.35,
             "Old req: 50%/#sqrt{E} + 5%", "New req: 35%/#sqrt{E} + 5%",
             "slide8_neutron_resolution.png", "slide8_neutron_resolution");

    printf("[DONE] Slides 7-8 complete\n");
}
