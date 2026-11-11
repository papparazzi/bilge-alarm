# JLCPCB tooling findings (2026-10-28)
- Economic PCBA accepted all 23 BOM lines; no "Standard Only" flag. Extended-part fee $18.54; "Large Size" fee $0 at 100 x 100 mm.
- U5 (MS5637, C3033010) is flagged "The processing of this component is difficult. After placing an order, it will be reviewed. If PCBA advanced option is required, additional fees will be charged."; it is unticked by default and has to be ticked by hand.
- BR1, BR2 and PW1 (bare pad rows) are not in the BOM on purpose; JLCPCB asks "Project has unselected parts" -> "Do not place".
- Placement viewer showed the SOT-23 (x7), TSOT-23-6 (U1) and SOIC-8W (U4) models rotated 180/270/90 degrees from the pads with raw KiCad rotations; the placement file now carries JLCPCB offsets (+180 SOT-23, +270 TSOT-23-6 and SOIC-8) and the viewer then matches pad 1 / silkscreen.
- Final draft price (5 boards, Economic, top side, Singapore, MS5637 ticked): PCB $4.00 + assembly $85.78 = $89.78, excluding shipping and the dev boards. Not added to the cart; no order placed.
- JLCPCB's own note: "File preview is for reference only. Check final part placement at DFM Analysis in Order Library in 4-6 hours after placing the order."
- PCB options left at JLCPCB defaults: FR-4 TG135, 1.6 mm, green, white silk, HASL with lead, 1 oz outer copper, tented vias, 0.3 mm min via drill.
