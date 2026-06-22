// plot_slide5_energy_dump.C — slide 5: energy dump scatter plots
// TProfile ECal/HCal/All vs beam energy, top row gamma, bottom row neutron
// Output: plots/slide5_energy_dump.png + plots/energy_reconstruction.root

TProfile* getProfile(TFile *f, const char* canvName, int padIdx) {
    TCanvas *c = (TCanvas*)f->Get(canvName);
    if (!c) return nullptr;
    int cnt = 0;
    TIter it(c->GetListOfPrimitives());
    TObject *obj;
    while ((obj = it())) {
        if (obj->InheritsFrom("TPad")) {
            if (cnt == padIdx) {
                TIter it2(((TPad*)obj)->GetListOfPrimitives());
                TObject *o2;
                while ((o2 = it2()))
                    if (o2->InheritsFrom("TProfile")) return (TProfile*)o2;
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
    p->SetMarkerSize(1.2);
    p->GetYaxis()->SetTitle(ytitle);
    p->GetXaxis()->SetTitle("Beam Energy (GeV)");
    p->GetXaxis()->SetTitleSize(0.055);
    p->GetYaxis()->SetTitleSize(0.055);
    p->GetXaxis()->SetLabelSize(0.050);
    p->GetYaxis()->SetLabelSize(0.050);
    p->GetXaxis()->SetTitleOffset(1.05);
    p->GetYaxis()->SetTitleOffset(1.30);
    p->GetYaxis()->SetMaxDigits(3);
}

void setupPad() {
    gPad->SetLeftMargin(0.17);
    gPad->SetBottomMargin(0.15);
    gPad->SetRightMargin(0.03);
    gPad->SetTopMargin(0.11);
    gPad->SetTicks(1,1);
}

void plot_slide5_energy_dump() {
    gStyle->SetOptStat(0);
    gStyle->SetPadGridX(1);
    gStyle->SetPadGridY(1);
    gStyle->SetTitleFontSize(0.07);

    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";

    TFile *fg = TFile::Open(TString::Format("%s/20260421_gamma_LYSO_diffAngle/res2_1_m0_w0_gamma.root",   base));
    TFile *fn = TFile::Open(TString::Format("%s/20260324_neutron_LYSO_diffAngle/res2_1_m0_w0_neutron.root", base));
    if (!fg || fg->IsZombie() || !fn || fn->IsZombie()) { printf("[ERROR] Cannot open files\n"); return; }

    // Pads 3,4,5 (0-indexed) are the TProfile objects for ECal, HCal, All
    TProfile *gEcal = getProfile(fg, "c_Eorg", 3);
    TProfile *gHcal = getProfile(fg, "c_Eorg", 4);
    TProfile *gAll  = getProfile(fg, "c_Eorg", 5);
    TProfile *nEcal = getProfile(fn, "c_Eorg", 3);
    TProfile *nHcal = getProfile(fn, "c_Eorg", 4);
    TProfile *nAll  = getProfile(fn, "c_Eorg", 5);

    if (!gEcal||!gHcal||!gAll||!nEcal||!nHcal||!nAll) {
        printf("[ERROR] Missing TProfile objects\n"); fg->Close(); fn->Close(); return;
    }

    int gc = kBlue+1, nc = kRed+1;
    styleProfile(gEcal, gc, "E_{ECAL} / E_{beam}", "Gamma (0.7-40 GeV):  E_{ECAL} / E_{beam}");
    styleProfile(gHcal, gc, "E_{HCAL} / E_{beam}", "Gamma:  E_{HCAL} / E_{beam}");
    styleProfile(gAll,  gc, "(E_{ECAL}+E_{HCAL}) / E_{beam}", "Gamma:  (E_{ECAL}+E_{HCAL}) / E_{beam}");
    styleProfile(nEcal, nc, "E_{ECAL} / E_{beam}", "Neutron (20-300 GeV):  E_{ECAL} / E_{beam}");
    styleProfile(nHcal, nc, "E_{HCAL} / E_{beam}", "Neutron:  E_{HCAL} / E_{beam}");
    styleProfile(nAll,  nc, "(E_{ECAL}+E_{HCAL}) / E_{beam}", "Neutron:  (E_{ECAL}+E_{HCAL}) / E_{beam}");

    // Set y-ranges
    gEcal->GetYaxis()->SetRangeUser(0.0, 0.65);
    gHcal->GetYaxis()->SetRangeUser(0.0, 0.021);
    gAll ->GetYaxis()->SetRangeUser(0.0, 0.65);
    nEcal->GetYaxis()->SetRangeUser(0.0, 0.060);
    nHcal->GetYaxis()->SetRangeUser(0.0, 0.021);
    nAll ->GetYaxis()->SetRangeUser(0.0, 0.060);

    TCanvas *c = new TCanvas("c_slide5","Energy Dump",2700,1500);
    c->Divide(3, 2, 0.003, 0.003);

    c->cd(1); setupPad(); gEcal->Draw("P E1");
    c->cd(2); setupPad(); gHcal->Draw("P E1");
    c->cd(3); setupPad(); gAll->Draw("P E1");
    c->cd(4); setupPad(); nEcal->Draw("P E1");
    c->cd(5); setupPad(); nHcal->Draw("P E1");
    c->cd(6); setupPad(); nAll->Draw("P E1");


    c->SaveAs(TString::Format("%s/plots/slide5_energy_dump.png", base));

    TFile *out = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base), "UPDATE");
    out->cd(); c->Write("slide5_energy_dump"); out->Close();

    fg->Close(); fn->Close();
    printf("[OK] slide5_energy_dump.png saved\n");
}
