// plot_slide7_8_resolution.C — slides 7-8: energy resolution + bias vs beam energy
// 3-panel layout: [resolution] [bias] [legend] — legend fully separated from data
// 6 method curves (m0-2 x w0-1), requirement TF1 lines on resolution plot
// Output: plots/slide7_gamma_resolution.png, slide8_neutron_resolution.png
//         + written to plots/energy_reconstruction.root

struct Curve {
    int m, w, color, marker;
    const char* label;
};

TGraphErrors* getGraph(const char* path, const char* name) {
    TFile *f = TFile::Open(path);
    if (!f || f->IsZombie()) return nullptr;
    TGraphErrors *g = (TGraphErrors*)f->Get(name);
    if (!g) { f->Close(); return nullptr; }
    TGraphErrors *gc = (TGraphErrors*)g->Clone(TString::Format("gc_%s_%s", name, gSystem->BaseName(path)));
    f->Close();
    return gc;
}

void styleDataPad(double lm=0.14, double bm=0.14, double rm=0.03, double tm=0.10) {
    gPad->SetLeftMargin(lm); gPad->SetBottomMargin(bm);
    gPad->SetRightMargin(rm); gPad->SetTopMargin(tm);
    gPad->SetTicks(1,1);
    gPad->SetGrid(1,1);
}

void styleAxes(TMultiGraph *mg, const char* xt, const char* yt, double ylo, double yhi) {
    mg->Draw("AP");
    mg->GetXaxis()->SetTitle(xt);
    mg->GetYaxis()->SetTitle(yt);
    mg->GetYaxis()->SetRangeUser(ylo, yhi);
    mg->GetXaxis()->SetTitleSize(0.054); mg->GetYaxis()->SetTitleSize(0.054);
    mg->GetXaxis()->SetLabelSize(0.048); mg->GetYaxis()->SetLabelSize(0.048);
    mg->GetXaxis()->SetTitleOffset(1.05); mg->GetYaxis()->SetTitleOffset(1.20);
    mg->GetYaxis()->SetMaxDigits(3);
}

void makePlot(
    const char* base, const char* particle, const char* dir,
    Curve *curves, int ncurves,
    double xlo, double xhi,
    double rlo, double rhi,
    double blo, double bhi,
    double reqOldP0, double reqNewP0,
    const char* reqOldLbl, const char* reqNewLbl,
    const char* outPng, const char* canvName
) {
    TMultiGraph *mgRes  = new TMultiGraph(TString::Format("mgRes_%s",  particle), "");
    TMultiGraph *mgBias = new TMultiGraph(TString::Format("mgBias_%s", particle), "");

    // Shared legend entries (filled during loop)
    struct LEntry { TObject *obj; const char* lbl; const char* opt; };
    std::vector<LEntry> legEntries;

    for (int i = 0; i < ncurves; i++) {
        Curve &cv = curves[i];
        TString path = TString::Format("%s/%s/res2_1_m%d_w%d_%s.root", base, dir, cv.m, cv.w, particle);
        TGraphErrors *grRes  = getGraph(path.Data(), "gReso_dcb");
        TGraphErrors *grBias = getGraph(path.Data(), "gBias_dcb");
        if (!grRes || !grBias) { printf("[WARN] missing m%d_w%d_%s\n",cv.m,cv.w,particle); continue; }

        for (TGraphErrors *g : {grRes, grBias}) {
            g->SetMarkerStyle(cv.marker); g->SetMarkerColor(cv.color);
            g->SetLineColor(cv.color);    g->SetMarkerSize(1.4);
            g->SetLineWidth(2);
        }
        mgRes->Add(grRes,   "PL");
        mgBias->Add(grBias, "PL");
        legEntries.push_back({grRes, cv.label, "p"});
    }

    // Requirement TF1s
    TF1 *fOld = new TF1(TString::Format("fOld_%s",particle), "[0]/sqrt(x)+[2]", xlo, xhi);
    fOld->SetParameters(reqOldP0, 0, 0.05);
    fOld->SetLineColor(kGray+2); fOld->SetLineWidth(3); fOld->SetLineStyle(7);

    TF1 *fNew = new TF1(TString::Format("fNew_%s",particle), "[0]/sqrt(x)+[2]", xlo, xhi);
    fNew->SetParameters(reqNewP0, 0, 0.05);
    fNew->SetLineColor(kOrange+2); fNew->SetLineWidth(3); fNew->SetLineStyle(2);

    legEntries.push_back({fOld, reqOldLbl, "l"});
    legEntries.push_back({fNew, reqNewLbl, "l"});

    // Canvas: 3 pads [resolution | bias | legend]
    TString partLabel = particle; partLabel[0] = toupper(partLabel[0]);
    TCanvas *c = new TCanvas(canvName, canvName, 3800, 1200);

    // Manual pad layout: res=0-41%, bias=42-76%, legend=77-100%
    TPad *pRes  = new TPad("pRes",  "", 0.00, 0.0, 0.41, 1.0); pRes->Draw();
    TPad *pBias = new TPad("pBias", "", 0.42, 0.0, 0.77, 1.0); pBias->Draw();
    TPad *pLeg  = new TPad("pLeg",  "", 0.78, 0.0, 1.00, 1.0); pLeg->Draw();

    // --- Resolution pad ---
    pRes->cd(); styleDataPad();
    TString titleRes = TString::Format("%s: Energy Resolution", partLabel.Data());
    mgRes->SetTitle(titleRes.Data());
    styleAxes(mgRes, "Beam Energy (GeV)", "#sigma (E_{rec} / E_{beam})", rlo, rhi);
    fOld->Draw("same"); fNew->Draw("same");
    gPad->RedrawAxis();

    // Title style
    pRes->GetListOfPrimitives()->FindObject("title");
    gStyle->SetTitleFontSize(0.065);

    // --- Bias pad ---
    pBias->cd(); styleDataPad();
    TString titleBias = TString::Format("%s: Energy Bias", partLabel.Data());
    mgBias->SetTitle(titleBias.Data());
    styleAxes(mgBias, "Beam Energy (GeV)", "#mu (E_{rec} / E_{beam})", blo, bhi);
    gPad->RedrawAxis();

    // --- Legend pad ---
    pLeg->cd();
    pLeg->SetFillColor(0); pLeg->SetBorderSize(0);
    TLegend *leg = new TLegend(0.03, 0.05, 0.97, 0.95);
    leg->SetBorderSize(1); leg->SetFillStyle(1001);
    leg->SetTextSize(0.063); leg->SetTextFont(42);
    leg->SetHeader(TString::Format("  %s Methods", partLabel.Data()), "C");
    for (auto &e : legEntries) leg->AddEntry(e.obj, e.lbl, e.opt);
    leg->Draw();

    c->SaveAs(TString::Format("%s/plots/%s", base, outPng));

    TFile *out = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base), "UPDATE");
    out->cd(); c->Write(canvName); out->Close();

    printf("[OK] %s saved\n", outPng);
    delete c;
}

void plot_slide7_8_resolution() {
    gStyle->SetOptStat(0);
    gStyle->SetTitleFontSize(0.065);

    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    Curve curves[] = {
        {0,0, kRed+1,     kFullCircle,     "Equal-Weighted,  Linear"},
        {1,0, kBlue+1,    kFullSquare,     "Equal-Weighted,  Ratio"},
        {2,0, kBlack,     kFullTriangleUp, "Equal-Weighted,  Quadratic"},
        {0,1, kGreen+2,   kOpenCircle,     "#sqrt{E}-Weighted,  Linear"},
        {1,1, kMagenta+1, kOpenSquare,     "#sqrt{E}-Weighted,  Ratio"},
        {2,1, kOrange+2,  kOpenTriangleUp, "#sqrt{E}-Weighted,  Quadratic"},
    };
    int nc = sizeof(curves)/sizeof(curves[0]);

    // Slide 7: Gamma (0.7-40 GeV)
    // Gamma bias actual range: 0.897 - 1.057 → use 0.87 - 1.10
    makePlot(base, "gamma", "20260421_gamma_LYSO_diffAngle", curves, nc,
             0.5, 50,
             0.0, 0.30,
             0.87, 1.10,
             0.35, 0.20,
             "Old req: 35%/#sqrt{E} + 5%", "New req: 20%/#sqrt{E} + 5%",
             "slide7_gamma_resolution.png", "slide7_gamma_resolution");

    // Slide 8: Neutron (20-300 GeV)
    // Neutron bias actual range: 1.01 - 3.57 → use 0.90 - 1.80 (shows most data; extreme 20 GeV outliers noted)
    makePlot(base, "neutron", "20260324_neutron_LYSO_diffAngle", curves, nc,
             10, 350,
             0.0, 0.45,
             0.90, 1.80,
             0.50, 0.35,
             "Old req: 50%/#sqrt{E} + 5%", "New req: 35%/#sqrt{E} + 5%",
             "slide8_neutron_resolution.png", "slide8_neutron_resolution");

    printf("[DONE] slides 7-8 complete\n");
}
