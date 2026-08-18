
## Documentation Files

| Filename                        | Notes                                    |
| ------------------------------- | ---------------------------------------- |
| README.pdf                      | This README file                         |
| VCU_Module-outline.dxf        | Board outline (with holes) in DXF format |
| VCU_Module-pcba.step          | 3D model of PCBA (with components)       |
| VCU_Module-render-bot.jpg     | Render of the top of the 3D model        |
| VCU_Module-render-bot.jpg     | Render of the bottom of the 3D model     |
| VCU_Module-schematic.pdf      | PDF of board schematics                  |

## Contact Information

- Website: *website*
- Email: *email*

## Board Renders

![Render of the top of the 3D model](./build/documentation/VCU_Module-render-top.jpg)
![Render of the bottom of the 3D model](./build/documentation/VCU_Module-render-bot.jpg)

\newpage

# Printed Circuit Board (PCB) Fabrication Information

---
title: "**Viking MotorSports VCU Module**"
subtitle: |
  **Fabrication and Assembly Information**\
  For build 2026-08-18T11-34-28
fontsize: 10pt
geometry:
  - margin=0.5in
toc: true
toc-depth: 2
colorlinks: true
urlcolor: blue
---

## Board Info

- 2 layer board
- Bounding box is *TBD*
- Board thickness is 1.59 mm (0.063 inch)

## Board Requirements

- Design Rules
    - Minimum Trace / Space design rulea: 0.1524 mm (6.0 mil) / 0.1524 mm (6.0 mil)
    - Outer dimension router tolerance: +/- 0.254 mm (10.0 mil)
    - Hole placement tolerance: +/- 0.075 mm (3.0 mil)
- Drills
   - Drill Positional Tolerance: 0.051 mm (2.0 mil)
   - Drill Size tolerance: +/- 0.064 mm (2.5 mil)
- Plated/Un-plated holes
  - There are no un-plated (NPT) holes
  - There are 96 plated through (PTH) holes
  - PTH minimum diameter: 0.254 mm (10 mil)
  - PTH minimum annulus: 0.102 mm (4 mil) radius
- Outline/Routing
  - No requirements.
- Slots
  - There are no slots.
- Cutouts
  - There are no cutouts
- There are 3 fiducials on the top layer.
- Panel tabs ("mouse bites")
   - Card edges must be smooth; no mouse bites or other intrusions into the card outline.
   - If external mouse bites are required, minimize and customer will remove by hand before assembly.
- If not otherwise specified, build to IPC 6012 Class 2 or better.

## Materials

- No requirements for materials.
- Copper Surface treatment should be ENIG, althogh immersion Silver is acceptable.
- White silkscreen on top and bottom surface
- Taiyo PSR-4000 or equivalent soldermask on top and bottom, no requirements for color.

## Stack Up

- Any FR4 like material is acceptable.
- 1 oz Copper on top and bottom layers
- 1.6 mm thick when done (1/16 inch)

## Array / Panel Information

- Coordinate with Contract Manufacturer (CM) for optimal size of this panel.
- If no feedback from CM, then produce single boards (no panel).

## Fabrication Files

### IPC-2581 File

| Filename                 | Notes                                   |
| ------------------------ | --------------------------------------- |
| VCU_Module-ipc2581.xml | IPC-2581 board information file         |

### Legacy PCB Files

| Filename                      | Notes                                         |
| ------------------------------| --------------------------------------------- |
| VCU_Module-Edge_Cuts.gbr    | RS274X file for the dimension (outline) layer |
| VCU_Module-F_Silkscreen.gbr | RS274X file for the top silkscreen            |
| VCU_Module-F_Mask.gbr       | RS274X file for the top soldermask            |
| VCU_Module-F_Cu.gbr         | RS274X file for the top copper layer          |
| VCU_Module-In1_Cu.gbr       | RS274X file for the layer 2 copper            |
| VCU_Module-In2_Cu.gbr       | RS274X file for the layer 3 copper            |
| VCU_Module-B_Cu.gbr         | RS274X file for the bottom copper layer       |
| VCU_Module-B_Mask.gbr       | RS274X file for the bottom soldermask         |
| VCU_Module-B_Silkscreen.gbr | RS274X file for the bottom silkscreen         |
| VCU_Module-NPTH.drl         | Excellon file for non-plated through holes    |
| VCU_Module-PTH.drl          | Excellon file for plated through holes        | 

\newpage

# Printed Circuit Board Assembly (PCBA) Information

## Assembly Info

- All components are on the top side of the board.
- This PCBA has two components that are SMT components with through-hole mechanical leads.

## Assembly Requirements

- Assemble to IPC Class 2 or better
- Solder paste can be ROHS or leaded.
- No clean, or Aqueous flux and wash, can be used.
- No conformal coating.

## Component Specific Assembly Information

- None

## Assembly Files

### IPC-2581 File

| Filename                 | Notes                           |
| ------------------------ | ------------------------------- |
| VCU_Module-ipc2581.xml | IPC-2581 board information file |

### Bill of Materials (BOM)

| Filename             | Description                            |
| -------------------- | -------------------------------------- |
| VCU_Module-bom.csv | BOM in Comma Separated Variable format |

### Solder Paste Stencils

| Filename                 | Notes                                            |
| ------------------------ | ------------------------------------------------ |
| VCU_Module-B_Paste.gbr | RS274X file for top/front solder paste stencil   |
| VCU_Module-F_Paste.gbr | RS274X file for bottom/back solder paste stencil |

### Mounting/Placement Location

| Filename            | Description                                 |
| ------------------- | ------------------------------------------- |
| VCU_Module-3u.pos | Pick and place locations for components     |

