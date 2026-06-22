// plot_slide6_erec_dist.C — slide 6: E_rec/E_beam distribution, 1 GeV gamma
// hERatioIndv_dcb[2] = 1.00 GeV gamma, linear method (m0), sqrt-E weighting (w1)
// Output: plots/slide6_erec_dist.png + plots/energy_reconstruction.root

void plot_slide6_erec_dist() {
    gStyle->SetOptStat(0);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);

    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";
    TFile *f = TFile::Open(TString::Format("%s/20260421_gamma_LYSO_diffAngle/res2_1_m0_w1_gamma.root", base));
    if (!f || f->IsZombie()) { printf("[ERROR] Cannot open gamma file\n"); return; }

    TCanvas *csrc = (TCanvas*)f->Get("c_Erec_dcb");
    if (!csrc) { printf("[ERROR] c_Erec_dcb not found\n"); f->Close(); return; }

    // Walk pads, find index 2 (0-based) = 1 GeV gamma
    TList *prims = csrc->GetListOfPrimitives();
    TIter it(prims);
    TObject *obj;
    int padIdx = 0;
    TH1D *h = nullptr;
    TString beamE="", mean="", sigma="", chi2="";

    while ((obj = it())) {
        if (obj->InheritsFrom("TPad") && padIdx == 2) {
            TPad *pad = (TPad*)obj;
            TIter it2(pad->GetListOfPrimitives());
            TObject *o2;
            while ((o2 = it2())) {
                if (o2->InheritsFrom("TH1")) h = (TH1D*)o2;
                if (TString(o2->ClassName()) == "TLatex") {
                    TString s = ((TLatex*)o2)->GetTitle();
                    if (s.Contains("BeamE")) beamE = s;
                    if (s.Contains("Mean"))  mean  = s;
                    if (s.Contains("Sigma")) sigma = s;
                    if (s.Contains("Chi2"))  chi2  = s;
                }
            }
            break;
        }
        if (obj->InheritsFrom("TPad")) padIdx++;
    }

    if (!h) { printf("[ERROR] histogram not found in pad 2\n"); f->Close(); return; }
    printf("[INFO] histogram: %s  entries=%.0f\n", h->GetName(), h->GetEntries());

    TCanvas *c = new TCanvas("c_slide6","E_rec/E_beam Distribution",1600,1200);
    c->SetLeftMargin(0.14);
    c->SetBottomMargin(0.13);
    c->SetRightMargin(0.05);
    c->SetTopMargin(0.08);

    h->SetLineColor(kBlack);
    h->SetFillColor(kWhite);
    h->GetXaxis()->SetTitle("E_{rec} / E_{beam}");
    h->GetYaxis()->SetTitle("Counts");
    h->GetXaxis()->SetRangeUser(0.0, 2.0);
    h->GetXaxis()->SetTitleSize(0.052);
    h->GetYaxis()->SetTitleSize(0.052);
    h->GetXaxis()->SetLabelSize(0.048);
    h->GetYaxis()->SetLabelSize(0.048);
    h->GetXaxis()->SetTitleOffset(1.05);
    h->GetYaxis()->SetTitleOffset(1.15);
    h->SetTitle("");
    h->Draw("HIST");

    // Draw stored DCB fit (red curve)
    TList *fns = h->GetListOfFunctions();
    if (fns && fns->GetSize() > 0) {
        TIter itf(fns);
        TObject *fn;
        while ((fn = itf())) {
            if (fn->InheritsFrom("TF1")) {
                TF1 *f1 = (TF1*)fn;
                f1->SetLineColor(kRed); f1->SetLineWidth(3);
                f1->Draw("same");
            }
        }
        printf("[INFO] fit function drawn\n");
    }

    // Stats box in RIGHT tail region — histogram very sparse at x > 1.3 GeV
    // peak at x~1.0, tail counts ~0-10 at x>1.4; box above the sparse bars
    double x0=0.64, y0=0.88, dy=0.085;
    TPaveText *box = new TPaveText(x0, y0-3.8*dy, 0.99, y0+0.03, "NDC");
    box->SetFillColor(0); box->SetBorderSize(1); box->SetTextAlign(12);
    box->SetTextSize(0.042); box->SetTextFont(42);
    if (!beamE.IsNull()) box->AddText(beamE);
    if (!mean.IsNull())  box->AddText(mean);
    if (!sigma.IsNull()) box->AddText(sigma);
    if (!chi2.IsNull())  box->AddText(chi2);
    box->Draw();

    c->SaveAs(TString::Format("%s/plots/slide6_erec_dist.png", base));

    TFile *out = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base), "UPDATE");
    out->cd(); c->Write("slide6_erec_dist"); out->Close();

    f->Close();
    printf("[OK] slide6_erec_dist.png saved\n");
}
