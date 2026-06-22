// plot_slide6_erec_dist.C
// Reproduces slide 6: E_rec/E_beam distribution for 1 GeV gamma
// hERatioIndv_dcb[2] = 1 GeV gamma (confirmed by TLatex inspection)
// Uses res2_1_m0_w1_gamma.root (linear method, sqrt-Ebeam weighting = best)
// Output: plots/slide6_erec_dist.png + written to plots/energy_reconstruction.root

void plot_slide6_erec_dist() {
    gStyle->SetOptStat(0);
    gStyle->SetPadTickX(1);
    gStyle->SetPadTickY(1);

    const char* base = "/mnt/c/Users/Julia/OneDrive/Desktop/coding/ASIoP/Explore MC-sim data for ZDC";
    TFile *f = TFile::Open(TString::Format("%s/20260421_gamma_LYSO_diffAngle/res2_1_m0_w1_gamma.root", base));
    if (!f || f->IsZombie()) { printf("[ERROR] Cannot open gamma file\n"); return; }

    // Navigate c_Erec_dcb canvas -> pad index 2 (0-based) -> hERatioIndv_dcb[2]
    TCanvas *csrc = (TCanvas*)f->Get("c_Erec_dcb");
    if (!csrc) { printf("[ERROR] c_Erec_dcb not found\n"); f->Close(); return; }

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
                    TLatex *lx = (TLatex*)o2;
                    TString s = lx->GetTitle();
                    if (s.Contains("BeamE"))  beamE = s;
                    if (s.Contains("Mean"))   mean  = s;
                    if (s.Contains("Sigma"))  sigma = s;
                    if (s.Contains("Chi2"))   chi2  = s;
                }
            }
            break;
        }
        if (obj->InheritsFrom("TPad")) padIdx++;
    }

    if (!h) { printf("[ERROR] hERatioIndv_dcb[2] not found in pad\n"); f->Close(); return; }
    printf("[INFO] Found histogram: %s  entries=%.0f\n", h->GetName(), h->GetEntries());

    TCanvas *c = new TCanvas("c_slide6","E_rec/E_beam Distribution",700,600);
    c->SetLeftMargin(0.12);
    c->SetBottomMargin(0.12);

    h->SetLineColor(kBlack);
    h->SetFillColor(kWhite);
    h->GetXaxis()->SetTitle("E_{rec} / E_{beam}");
    h->GetYaxis()->SetTitle("Count");
    h->GetXaxis()->SetRangeUser(0.0, 2.0);
    h->GetXaxis()->SetTitleSize(0.05);
    h->GetYaxis()->SetTitleSize(0.05);
    h->SetTitle("");
    h->Draw("HIST");

    // Draw any stored fit functions (DCB fit stored in histogram's function list)
    TList *fns = h->GetListOfFunctions();
    if (fns && fns->GetSize() > 0) {
        printf("[INFO] Found %d fit function(s) in histogram\n", fns->GetSize());
        TIter itf(fns);
        TObject *fn;
        while ((fn = itf())) {
            if (fn->InheritsFrom("TF1")) {
                TF1 *f1 = (TF1*)fn;
                f1->SetLineColor(kRed);
                f1->SetLineWidth(2);
                f1->Draw("same");
            }
        }
    } else {
        printf("[WARN] No fit function stored in histogram — drawing histogram only\n");
    }

    // Add info labels (from stored TLatex strings)
    double x0 = 0.55, y0 = 0.82, dy = 0.07;
    auto addLabel = [&](TString s, double y) {
        if (s.IsNull()) return;
        TLatex *lx = new TLatex(x0, y, s);
        lx->SetNDC(); lx->SetTextSize(0.04); lx->Draw();
    };
    addLabel(beamE, y0);
    addLabel(mean,  y0 - dy);
    addLabel(sigma, y0 - 2*dy);
    addLabel(chi2,  y0 - 3*dy);

    // Box around labels
    TPaveText *box = new TPaveText(x0-0.01, y0-3.5*dy, 0.98, y0+0.04, "NDC");
    box->SetFillStyle(0); box->SetBorderSize(1);
    box->Draw();

    c->SaveAs(TString::Format("%s/plots/slide6_erec_dist.png", base));

    TFile *out = TFile::Open(TString::Format("%s/plots/energy_reconstruction.root", base), "UPDATE");
    out->cd();
    c->Write("slide6_erec_dist");
    out->Close();

    f->Close();
    printf("[OK] slide6_erec_dist.png saved\n");
}
