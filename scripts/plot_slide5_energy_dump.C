// plot_slide5_energy_dump.C
// Reproduces slide 5: Energy Dump scatter plots
// Top row: 1-40 GeV Gamma   Bottom row: 20-300 GeV Neutron
// Uses TProfile objects from c_Eorg canvas (pads 4,5,6)
// Output: plots/slide5_energy_dump.png + plots/energy_reconstruction.root
// Usage: root -b -q scripts/plot_slide5_energy_dump.C

void setStyle() {
    gStyle->SetOptStat(0);
    gStyle->SetPadGridX(1);
    gStyle->SetPadGridY(1);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);
    gStyle->SetTitleFontSize(0.06);
    gStyle->SetLabelSize(0.05,"xyz");
    gStyle->SetTitleSize(0.055,"xyz");
}

TProfile* getProfile(TFile *f, const char* canvName, int padIdx) {
    // padIdx: 0-based index into canvas pad list
    TCanvas *c = (TCanvas*)f->Get(canvName);
    if (!c) { printf("[ERROR] Canvas %s not found\n", canvName); return nullptr; }
    TList *prims = c->GetListOfPrimitives();
    int cnt = 0;
    TIter it(prims);
    TObject *obj;
    while ((obj = it())) {
        if (obj->InheritsFrom("TPad")) {
            if (cnt == padIdx) {
                TPad *pad = (TPad*)obj;
                TIter it2(pad->GetListOfPrimitives());
                TObject *o2;
                while ((o2 = it2())) {
                    if (o2->InheritsFrom("TProfile")) return (TProfile*)o2;
                }
            }
            cnt++;
        }
    }
    return nullptr;
}

void styleProfile(TProfile *p, int color, const char* ytitle, const char* title) {
    p->SetTitle(title);
    p->SetMarkerStyle(kOpenCircle);
    p->SetMarkerColor(color);
    p->SetLineColor(color);
    p->SetMarkerSize(1.0);
    p->GetYaxis()->SetTitle(ytitle);
    p->GetXaxis()->SetTitle("Beam (GeV)");
    p->GetXaxis()->SetTitleOffset(1.1);
    p->GetYaxis()->SetTitleOffset(1.3);
}

void plot_slide5_energy_dump() {
    setStyle();
    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    TFile *fg = TFile::Open(TString::Format("%s/20260421_gamma_LYSO_diffAngle/res2_1_m0_w0_gamma.root", base));
    TFile *fn = TFile::Open(TString::Format("%s/20260324_neutron_LYSO_diffAngle/res2_1_m0_w0_neutron.root", base));
    if (!fg || fg->IsZombie() || !fn || fn->IsZombie()) {
        printf("[ERROR] Cannot open input files\n"); return;
    }

    // Extract TProfile objects — pads 3,4,5 (0-indexed) = ECAL, HCAL, All
    TProfile *gEcal = getProfile(fg, "c_Eorg", 3);
    TProfile *gHcal = getProfile(fg, "c_Eorg", 4);
    TProfile *gAll  = getProfile(fg, "c_Eorg", 5);
    TProfile *nEcal = getProfile(fn, "c_Eorg", 3);
    TProfile *nHcal = getProfile(fn, "c_Eorg", 4);
    TProfile *nAll  = getProfile(fn, "c_Eorg", 5);

    if (!gEcal || !gHcal || !gAll || !nEcal || !nHcal || !nAll) {
        printf("[ERROR] Missing TProfile objects — check pad indices\n");
        fg->Close(); fn->Close(); return;
    }

    int col = kBlue+1;
    styleProfile(gEcal, col, "ECal / Beam", "Gamma: Beam VS ECAL/Beam");
    styleProfile(gHcal, col, "HCal / Beam", "Gamma: Beam VS HCAL/Beam");
    styleProfile(gAll,  col, "(ECal+HCal) / Beam", "Gamma: Beam VS (ECAL+HCAL)/Beam");
    styleProfile(nEcal, kRed+1, "ECal / Beam", "Neutron: Beam VS ECAL/Beam");
    styleProfile(nHcal, kRed+1, "HCal / Beam", "Neutron: Beam VS HCAL/Beam");
    styleProfile(nAll,  kRed+1, "(ECal+HCal) / Beam", "Neutron: Beam VS (ECAL+HCAL)/Beam");

    // Fix y-ranges to match slide
    gEcal->GetYaxis()->SetRangeUser(0.0, 0.65);
    gHcal->GetYaxis()->SetRangeUser(0.0, 0.020);
    gAll->GetYaxis()->SetRangeUser(0.0, 0.65);
    nEcal->GetYaxis()->SetRangeUser(0.0, 0.06);
    nHcal->GetYaxis()->SetRangeUser(0.0, 0.020);
    nAll->GetYaxis()->SetRangeUser(0.0, 0.06);

    TCanvas *c = new TCanvas("c_slide5","Energy Dump",1800,1000);
    c->Divide(3, 2, 0.005, 0.005);

    c->cd(1); gPad->SetLeftMargin(0.16); gPad->SetBottomMargin(0.14);
    gEcal->Draw("P E1"); gPad->SetTicks(1,1);

    c->cd(2); gPad->SetLeftMargin(0.16); gPad->SetBottomMargin(0.14);
    gHcal->Draw("P E1");

    c->cd(3); gPad->SetLeftMargin(0.16); gPad->SetBottomMargin(0.14);
    gAll->Draw("P E1");

    c->cd(4); gPad->SetLeftMargin(0.16); gPad->SetBottomMargin(0.14);
    nEcal->Draw("P E1");

    c->cd(5); gPad->SetLeftMargin(0.16); gPad->SetBottomMargin(0.14);
    nHcal->Draw("P E1");

    c->cd(6); gPad->SetLeftMargin(0.16); gPad->SetBottomMargin(0.14);
    nAll->Draw("P E1");

    // Row labels
    c->cd(1);
    TLatex *ltg = new TLatex(0.18, 0.85, "1 - 40 GeV Gamma");
    ltg->SetNDC(); ltg->SetTextSize(0.055); ltg->SetTextColor(kBlue+1); ltg->Draw();
    c->cd(4);
    TLatex *ltn = new TLatex(0.18, 0.85, "20 - 300 GeV Neutron");
    ltn->SetNDC(); ltn->SetTextSize(0.055); ltn->SetTextColor(kRed+1); ltn->Draw();

    c->SaveAs(TString::Format("%s/plots/slide5_energy_dump.png", base));

    // Write to TBrowser-friendly output file
    TFile *out = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base), "UPDATE");
    out->cd();
    c->Write("slide5_energy_dump");
    out->Close();

    fg->Close(); fn->Close();
    printf("[OK] slide5_energy_dump.png saved\n");
}
