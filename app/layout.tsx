import type { Metadata } from "next";
import "./globals.css";

import Navbar from "@/components/Navbar";
import Footer from "@/components/Footer";
import SponsorModal from "@/components/SponsorModal";

export const metadata: Metadata = {
  title: "NeoCalculator — NumOS | The €20 Open-Source Graphing Calculator",
  description: "NumOS: Open-source scientific OS running CAS symbolic algebra, real-time physics, and 17 apps on a $5 ESP32-S3. Disrupting the $150 calculator monopoly.",
  keywords: [
    "NeoCalculator",
    "NumOS",
    "Open-source graphing calculator",
    "CAS engine",
    "ESP32-S3",
    "Symbolic algebra",
    "Open hardware",
  ],
  metadataBase: new URL("https://neocalculator.tech"),
  openGraph: {
    type: "website",
    title: "NeoCalculator: The €20 Open-Source Graphing Calculator",
    description: "NumOS runs a full CAS engine, real-time Navier-Stokes physics, and 17 university-grade apps on a $5 ESP32-S3 with 97 KB RAM. Open-source hardware disrupting the $150 calculator monopoly.",
    url: "https://neocalculator.tech",
    images: [
      {
        url: "https://neocalculator.tech/og-image.png",
        width: 1200,
        height: 630,
        alt: "NeoCalculator",
      },
    ],
    siteName: "NeoCalculator",
  },
  twitter: {
    card: "summary_large_image",
    title: "NeoCalculator: The €20 Open-Source Graphing Calculator",
    description: "NumOS: CAS engine + 17 apps on $5 silicon. Open-source hardware that surpasses a TI-84 at 8× lower cost.",
    images: ["https://neocalculator.tech/og-image.png"],
  },
  icons: {
    icon: "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>🔢</text></svg>",
  },
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en">
      <head>
        <link rel="preconnect" href="https://fonts.googleapis.com" />
        <link rel="preconnect" href="https://fonts.gstatic.com" crossOrigin="" />
        <link
          href="https://fonts.googleapis.com/css2?family=Inter:wght@400;600;800;900&family=JetBrains+Mono:wght@400;500;700&display=swap"
          rel="stylesheet"
        />
        <link
          rel="stylesheet"
          href="https://cdn.jsdelivr.net/npm/katex@0.16.9/dist/katex.min.css"
        />
      </head>
      <body className="antialiased">
        <Navbar />
        {children}
        <Footer />
        <SponsorModal />
      </body>
    </html>
  );
}
