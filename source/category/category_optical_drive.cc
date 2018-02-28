//
// PROJECT:         Aspia
// FILE:            category/category_optical_drive.cc
// LICENSE:         GNU Lesser General Public License 2.1
// PROGRAMMERS:     Dmitry Chapyshev (dmitry@aspia.ru)
//

#include "base/devices/device_spti.h"
#include "base/files/logical_drive_enumerator.h"
#include "base/strings/string_printf.h"
#include "base/strings/string_util.h"
#include "base/byte_order.h"
#include "base/logging.h"
#include "category/category_optical_drive.h"
#include "category/category_optical_drive.pb.h"
#include "ui/resource.h"

namespace aspia {

namespace {

struct ScsiVendor
{
    const char* vendor_code;
    const char* vendor_name;
};

const ScsiVendor kScsiVendors[] =
{
    { "0B4C", "MOOSIK Ltd." },
    { "13FE", "PHISON" },
    { "2AI", "2AI (Automatisme et Avenir Informatique)" },
    { "3M", "3M Company" },
    { "3nhtech", "3NH Technologies" },
    { "3PARdata", "3PARdata, Inc. (now HP)" },
    { "A-Max", "A-Max Technology Co., Ltd" },
    { "ABSOLUTE", "Absolute Analysis" },
    { "ACARD", "ACARD Technology Corp." },
    { "Accusys", "Accusys INC." },
    { "Acer", "Acer, Inc." },
    { "ACL", "Automated Cartridge Librarys, Inc." },
    { "Actifio", "Actifio" },
    { "Acuid", "Acuid Corporation Ltd." },
    { "AcuLab", "AcuLab, Inc. (Tulsa, OK)" },
    { "ADAPTEC", "Adaptec (now PMC-Sierra)" },
    { "ADIC", "Advanced Digital Information Corporation" },
    { "ADSI", "Adaptive Data Systems, Inc. (a Western Digital subsidiary)" },
    { "ADTX", "ADTX Co., Ltd." },
    { "ADVA", "ADVA Optical Networking AG" },
    { "AEM", "AEM Performance Electronics" },
    { "AERONICS", "Aeronics, Inc." },
    { "AGFA", "AGFA" },
    { "Agilent", "Agilent Technologies" },
    { "AIC", "Advanced Industrial Computer, Inc." },
    { "AIPTEK", "AIPTEK International Inc." },
    { "Alcohol", "Alcohol Soft" },
    { "ALCOR", "Alcor Micro, Corp." },
    { "AMCC", "Applied Micro Circuits Corporation" },
    { "AMCODYNE", "Amcodyne" },
    { "Amgeon", "Amgeon LLC" },
    { "AMI", "American Megatrends, Inc." },
    { "AMPEX", "Ampex Data Systems" },
    { "Amphenol", "Amphenol" },
    { "Amtl", "Tenlon Technology Co.,Ltd" },
    { "ANAMATIC", "Anamartic Limited (England)" },
    { "Ancor", "Ancor Communications, Inc." },
    { "ANCOT", "ANCOT Corp." },
    { "ANDATACO", "Andataco (now nStor)" },
    { "andiamo", "Andiamo Systems, Inc." },
    { "ANOBIT", "Anobit" },
    { "ANRITSU", "Anritsu Corporation" },
    { "ANTONIO", "Antonio Precise Products Manufactory Ltd." },
    { "AoT", "Art of Technology AG" },
    { "APPLE", "Apple Computer, Inc." },
    { "ARCHIVE", "Archive" },
    { "ARDENCE", "Ardence Inc" },
    { "Areca", "Areca Technology Corporation" },
    { "Arena", "MaxTronic International Co., Ltd." },
    { "Argent", "Argent Data Systems, Inc." },
    { "ARIO", "Ario Data Networks, Inc." },
    { "ARISTOS", "Aristos Logic Corp. (now part of PMC Sierra)" },
    { "ARK", "ARK Research Corporation" },
    { "ARL:UT@A", "Applied Research Laboratories : University of Texas at Austin" },
    { "ARTECON", "Artecon Inc. (Obs. - now Dot Hill)" },
    { "Artistic", "Artistic Licence (UK) Ltd" },
    { "ARTON", "Arton Int." },
    { "ASACA", "ASACA Corp." },
    { "ASC", "Advanced Storage Concepts, Inc." },
    { "ASPEN", "Aspen Peripherals" },
    { "AST", "AST Research" },
    { "ASTEK", "Astek Corporation" },
    { "ASTK", "Alcatel STK A/S" },
    { "AStor", "AccelStor, Inc." },
    { "ASTRO", "ASTRODESIGN,Inc." },
    { "ASTUTE", "Astute Networks, Inc." },
    { "AT&T", "AT&T" },
    { "ATA", "SCSI / ATA Translator Software (Organization Not Specified)" },
    { "ATARI", "Atari Corporation" },
    { "ATech", "ATech electronics" },
    { "ATG CYG", "ATG Cygnet Inc." },
    { "ATL", "Quantum|ATL Products" },
    { "ATTO", "ATTO Technology Inc." },
    { "ATTRATEC", "Attratech Ltd liab. Co" },
    { "ATX", "Alphatronix" },
    { "Audio3", "Audio3 Ltd." },
    { "AURASEN", "Aurasen Limited" },
    { "Avago", "Avago Technologies" },
    { "AVC", "AVC Technology Ltd" },
    { "AVIDVIDR", "AVID Technologies, Inc." },
    { "AVR", "Advanced Vision Research" },
    { "AXSTOR", "AXSTOR", },
    { "Axxana", "Axxana Ltd.", },
    { "B*BRIDGE", "Blockbridge Networks LLC", },
    { "BALLARD", "Ballard Synergy Corp." },
    { "Barco", "Barco" },
    { "BAROMTEC", "Barom Technologies Co., Ltd." },
    { "Bassett", "Bassett Electronic Systems Ltd" },
    { "BC Hydro", "BC Hydro" },
    { "BDT", "BDT AG" },
    { "BECEEM", "Beceem Communications, Inc" },
    { "BENQ", "BENQ Corporation." },
    { "BERGSWD", "Berg Software Design" },
    { "BEZIER", "Bezier Systems, Inc." },
    { "BHTi", "Breece Hill Technologies" },
    { "biodata", "Biodata Devices SL" },
    { "BIOS", "BIOS Corporation" },
    { "BIR", "Bio-Imaging Research, Inc." },
    { "BiT", "BiT Microsystems (obsolete, new ID: BITMICRO)" },
    { "BITMICRO", "BiT Microsystems, Inc." },
    { "Blendlgy", "Blendology Limited" },
    { "BLOOMBAS", "Bloombase Technologies Limited" },
    { "BlueArc", "BlueArc Corporation" },
    { "bluecog", "bluecog" },
    { "BME-HVT", "Broadband Infocommunicatons and Electromagnetic Theory Department" },
    { "BNCHMARK", "Benchmark Tape Systems Corporation" },
    { "Bosch", "Robert Bosch GmbH" },
    { "Botman", "Botmanfamily Electronics" },
    { "BoxHill", "Box Hill Systems Corporation (Obs. - now Dot Hill)" },
    { "BRDGWRKS", "Bridgeworks Ltd." },
    { "BREA", "BREA Technologies, Inc." },
    { "BREECE", "Breece Hill LLC" },
    { "BreqLabs", "BreqLabs Inc." },
    { "Broadcom", "Broadcom Corporation" },
    { "BROCADE", "Brocade Communications Systems, Incorporated" },
    { "BUFFALO", "BUFFALO INC." },
    { "BULL", "Bull Peripherals Corp." },
    { "BUSLOGIC", "BusLogic Inc." },
    { "BVIRTUAL", "B-Virtual N.V." },
    { "CACHEIO", "CacheIO LLC" },
    { "CalComp", "CalComp, A Lockheed Company" },
    { "CALCULEX", "CALCULEX, Inc." },
    { "CALIPER", "Caliper (California Peripheral Corp.)" },
    { "CAMBEX", "Cambex Corporation" },
    { "CAMEOSYS", "Cameo Systems Inc." },
    { "CANDERA", "Candera Inc." },
    { "CAPTION", "CAPTION BANK" },
    { "CAST", "Advanced Storage Tech" },
    { "CATALYST", "Catalyst Enterprises" },
    { "CCDISK", "iSCSI Cake" },
    { "CDC", "Control Data or MPI" },
    { "CDP", "Columbia Data Products" },
    { "CDS", "Cirrus Data Solutions, Inc." },
    { "Celsia", "A M Bromley Limited" },
    { "CenData", "Central Data Corporation" },
    { "Cereva", "Cereva Networks Inc." },
    { "CERTANCE", "Certance" },
    { "Chantil", "Chantil Technology" },
    { "CHEROKEE", "Cherokee Data Systems" },
    { "CHINON", "Chinon" },
    { "CHPSYSTM", "Cheapie Systems" },
    { "CHRISTMA", "Christmann Informationstechnik + Medien GmbH & Co KG" },
    { "CIE&YED", "YE Data, C.Itoh Electric Corp." },
    { "CIPHER", "Cipher Data Products" },
    { "Ciprico", "Ciprico, Inc." },
    { "CIRRUSL", "Cirrus Logic Inc." },
    { "CISCO", "Cisco Systems, Inc." },
    { "CLEARSKY", "ClearSky Data, Inc." },
    { "CLOVERLF", "Cloverleaf Communications, Inc" },
    { "CLS", "Celestica" },
    { "CMD", "CMD Technology Inc." },
    { "CMTechno", "CMTech" },
    { "CNGR SFW", "Congruent Software, Inc." },
    { "CNSi", "Chaparral Network Storage, Inc." },
    { "CNT", "Computer Network Technology" },
    { "COBY", "Coby Electronics Corporation, USA" },
    { "COGITO", "Cogito" },
    { "COMAY", "Corerise Electronics" },
    { "COMPAQ", "Compaq Computer Corporation (now HP)" },
    { "COMPELNT", "Compellent Technologies, Inc. (now Dell)" },
    { "COMPORT", "Comport Corp." },
    { "COMPSIG", "Computer Signal Corporation" },
    { "COMPTEX", "Comptex Pty Limited" },
    { "CONNER", "Conner Peripherals" },
    { "Control", "Controlant ehf." },
    { "COPANSYS", "COPAN SYSTEMS INC" },
    { "CORAID", "Coraid, Inc" },
    { "CORE", "Core International, Inc." },
    { "CORERISE", "Corerise Electronics" },
    { "COVOTE", "Covote GmbH & Co KG" },
    { "COWON", "COWON SYSTEMS, Inc." },
    { "CPL", "Cross Products Ltd" },
    { "CPU TECH", "CPU Technology, Inc." },
    { "CREO", "Creo Products Inc." },
    { "CROSFLD", "Crosfield Electronics (now FujiFilm Electonic Imaging Ltd)" },
    { "CROSSRDS", "Crossroads Systems, Inc." },
    { "crosswlk", "Crosswalk, Inc." },
    { "CSCOVRTS", "Cisco - Veritas" },
    { "CSM, INC", "Computer SM, Inc." },
    { "CULTECH", "Cultech Trading Ltd" },
    { "Cunuqui", "CUNUQUI SLU" },
    { "CYBERNET", "Cybernetics" },
    { "Cygnal", "Dekimo" },
    { "CYPRESS", "Cypress Semiconductor Corp." },
    { "D Bit", "Digby's Bitpile, Inc. DBA D Bit" },
    { "DALSEMI", "Dallas Semiconductor" },
    { "DANEELEC", "Dane-Elec" },
    { "DANGER", "Danger Inc." },
    { "DAT-MG", "DAT Manufacturers Group" },
    { "Data Com", "Data Com Information Systems Pty. Ltd." },
    { "DATABOOK", "Databook, Inc." },
    { "DATACOPY", "Datacopy Corp." },
    { "DataCore", "DataCore Software Corporation" },
    { "DataG", "DataGravity" },
    { "DATAPT", "Datapoint Corp." },
    { "DATARAM", "Dataram Corporation" },
    { "DATC", "Datum Champion Technology Co., Ltd" },
    { "DAVIS", "Daviscomms (S) Pte Ltd" },
    { "DCS", "ShenZhen DCS Group Co.,Ltd" },
    { "DDN", "DataDirect Networks, Inc." },
    { "DDRDRIVE", "DDRdrive LLC" },
    { "DE", "Dimension Engineering LLC" },
    { "DEC", "Digital Equipment Corporation (now HP)" },
    { "DEI", "Digital Engineering, Inc." },
    { "DELL", "Dell, Inc." },
    { "Dell(tm)", "Dell, Inc" },
    { "DellEMC", "Dell EMC" },
    { "DELPHI", "Delphi Data Div. of Sparks Industries, Inc." },
    { "DENON", "Denon/Nippon Columbia" },
    { "DenOptix", "DenOptix, Inc." },
    { "DEST", "DEST Corp." },
    { "DFC", "DavioFranke.com" },
    { "DFT", "Data Fault Tolerance System CO.,LTD." },
    { "DGC", "Data General Corp." },
    { "DIGIDATA", "Digi-Data Corporation" },
    { "DigiIntl", "Digi International" },
    { "Digital", "Digital Equipment Corporation (now HP)" },
    { "DILOG", "Distributed Logic Corp." },
    { "DISC", "Document Imaging Systems Corp." },
    { "DiscSoft", "Disc Soft Ltd" },
    { "DLNET", "Driveline" },
    { "DNS", "Data and Network Security" },
    { "DNUK", "Digital Networks Uk Ltd" },
    { "DotHill", "Dot Hill Systems Corp." },
    { "DP", "Dell, Inc." },
    { "DPT", "Distributed Processing Technology" },
    { "Drewtech", "Drew Technologies, Inc." },
    { "DROBO", "Data Robotics, Inc." },
    { "DSC", "DigitalStream Corporation" },
    { "DSI", "Data Spectrum, Inc." },
    { "DSM", "Deterner Steuerungs- und Maschinenbau GmbH & Co." },
    { "DSNET", "Cleversafe, Inc." },
    { "DT", "Double-Take Software, INC." },
    { "DTC QUME", "Data Technology Qume" },
    { "DXIMAGIN", "DX Imaging" },
    { "E-Motion", "E-Motion LLC" },
    { "EARTHLAB", "EarthLabs" },
    { "EarthLCD", "Earth Computer Technologies, Inc." },
    { "ECCS", "ECCS, Inc." },
    { "ECMA", "European Computer Manufacturers Association" },
    { "EDS", "Embedded Data Systems" },
    { "EIM", "InfoCore" },
    { "ELE Intl", "ELE International" },
    { "ELEGANT", "Elegant Invention, LLC" },
    { "Elektron", "Elektron Music Machines MAV AB" },
    { "elipsan", "Elipsan UK Ltd." },
    { "Elms", "Elms Systems Corporation" },
    { "ELSE", "ELSE Ltd." },
    { "ELSEC", "Littlemore Scientific" },
    { "EMASS", "EMASS, Inc." },
    { "EMC", "EMC Corp." },
    { "EMiT", "EMiT Conception Eletronique" },
    { "EMTEC", "EMTEC Magnetics" },
    { "EMULEX", "Emulex" },
    { "ENERGY-B", "Energybeam Corporation" },
    { "ENGENIO", "Engenio Information Technologies, Inc." },
    { "ENMOTUS", "Enmotus Inc" },
    { "Entacore", "Entacore" },
    { "EPOS", "EPOS Technologies Ltd." },
    { "EPSON", "Epson" },
    { "EQLOGIC", "EqualLogic" },
    { "Eris/RSI", "RSI Systems, Inc." },
    { "ETERNE", "EterneData Technology Co.,Ltd.(China PRC.)" },
    { "EuroLogc", "Eurologic Systems Limited (now part of PMC Sierra)" },
    { "evolve", "Evolution Technologies, Inc" },
    { "EXABYTE", "Exabyte Corp. (now part of Tandberg)" },
    { "EXATEL", "Exatelecom Co., Ltd." },
    { "EXAVIO", "Exavio, Inc." },
    { "Exsequi", "Exsequi Ltd" },
    { "Exxotest", "Annecy Electronique" },
    { "FAIRHAVN", "Fairhaven Health, LLC" },
    { "FALCON", "FalconStor, Inc." },
    { "FDS", "Formation Data Systems" },
    { "FFEILTD", "FujiFilm Electonic Imaging Ltd" },
    { "Fibxn", "Fiberxon, Inc." },
    { "FID", "First International Digital, Inc." },
    { "FILENET", "FileNet Corp." },
    { "FirmFact", "Firmware Factory Ltd" },
    { "FLYFISH", "Flyfish Technologies" },
    { "FOXCONN", "Foxconn Technology Group" },
    { "FRAMDRV", "FRAMEDRIVE Corp." },
    { "FREECION", "Nable Communications, Inc." },
    { "FRESHDTK", "FreshDetect GmbH" },
    { "FSC", "Fujitsu Siemens Computers" },
    { "FTPL", "Frontline Technologies Pte Ltd" },
    { "FUJI", "Fuji Electric Co., Ltd. (Japan)" },
    { "FUJIFILM", "Fuji Photo Film, Co., Ltd." },
    { "FUJITSU", "Fujitsu" },
    { "FUNAI", "Funai Electric Co., Ltd." },
    { "FUSIONIO", "Fusion-io Inc." },
    { "FUTURED", "Future Domain Corp." },
    { "G&D", "Giesecke & Devrient GmbH" },
    { "G.TRONIC", "Globaltronic - Electronica e Telecomunicacoes, S.A." },
    { "Gadzoox", "Gadzoox Networks, Inc. (now part of Broadcom)" },
    { "Gammaflx", "Gammaflux L.P." },
    { "GDI", "Generic Distribution International" },
    { "GEMALTO", "gemalto" },
    { "Gen_Dyn", "General Dynamics" },
    { "Generic", "Generic Technology Co., Ltd." },
    { "GENSIG", "General Signal Networks" },
    { "GEO", "Green Energy Options Ltd" },
    { "GIGATAPE", "GIGATAPE GmbH" },
    { "GIGATRND", "GigaTrend Incorporated" },
    { "Global", "Global Memory Test Consortium" },
    { "Gnutek", "Gnutek Ltd." },
    { "Goidelic", "Goidelic Precision, Inc." },
    { "GoldKey", "GoldKey Security Corporation" },
    { "GoldStar", "LG Electronics Inc." },
    { "GOOGLE", "Google, Inc." },
    { "GORDIUS", "Gordius" },
    { "Gossen", "Gossen Foto- und Lichtmesstechnik GmbH" },
    { "GOULD", "Gould" },
    { "HAGIWARA", "Hagiwara Sys-Com Co., Ltd." },
    { "HAPP3", "Inventec Multimedia and Telecom co., ltd" },
    { "HDS", "Horizon Data Systems, Inc." },
    { "Helldyne", "Helldyne, Inc" },
    { "Heydays", "Mazo Technology Co., Ltd." },
    { "HGST", "HGST a Western Digital Company" },
    { "HI-TECH", "HI-TECH Software Pty. Ltd." },
    { "HITACHI", "Hitachi America Ltd or Nissei Sangyo America Ltd" },
    { "HL-DT-ST", "Hitachi-LG Data Storage, Inc." },
    { "HONEYWEL", "Honeywell Inc." },
    { "Hoptroff", "HexWax Ltd" },
    { "HORIZONT", "Horizontigo Software" },
    { "HP", "Hewlett Packard" },
    { "HPE", "Hewlett Packard Enterprise" },
    { "HPI", "HP Inc." },
    { "HPQ", "Hewlett Packard" },
    { "HUALU", "CHINA HUALU GROUP CO., LTD" },
    { "HUASY", "Huawei Symantec Technologies Co., Ltd." },
    { "HUAWEI", "Huawei Technologies Co. Ltd." },
    { "HYLINX", "Hylinx Ltd." },
    { "HYUNWON", "HYUNWON inc" },
    { "i-cubed", "i-cubed ltd." },
    { "IBM", "International Business Machines" },
    { "Icefield", "Icefield Tools Corporation" },
    { "Iceweb", "Iceweb Storage Corp" },
    { "ICL", "ICL" },
    { "ICP", "ICP vortex Computersysteme GmbH" },
    { "IDE", "International Data Engineering, Inc." },
    { "IDG", "Interface Design Group" },
    { "IET", "ISCSI ENTERPRISE TARGET" },
    { "IFT", "Infortrend Technology, Inc." },
    { "IGR", "Intergraph Corp." },
    { "IMAGINE", "Imagine Communications Corp." },
    { "IMAGO", "IMAGO SOFTWARE SL" },
    { "IMATION", "Imation" },
    { "IMPLTD", "Integrated Micro Products Ltd." },
    { "IMPRIMIS", "Imprimis Technology Inc." },
    { "INCIPNT", "Incipient Technologies Inc." },
    { "INCITS", "InterNational Committee for Information Technology" },
    { "INDCOMP", "Industrial Computing Limited" },
    { "Indigita", "Indigita Corporation" },
    { "INFOCORE", "InfoCore" },
    { "INITIO", "Initio Corporation" },
    { "INNES", "INNES" },
    { "INRANGE", "INRANGE Technologies Corporation" },
    { "Insight", "L-3 Insight Technology Inc" },
    { "INSITE", "Insite Peripherals" },
    { "INSPUR", "Inspur Electronic Information Industry Co.,Ltd." },
    { "integrix", "Integrix, Inc." },
    { "INTEL", "Intel Corporation" },
    { "Intransa", "Intransa, Inc." },
    { "INTRMODL", "InterModal Data, Inc" },
    { "IOC", "I/O Concepts, Inc." },
    { "ioFABRIC", "ioFABRIC Inc." },
    { "iofy", "iofy Corporation" },
    { "IOMEGA", "Iomega" },
    { "IOT", "IO Turbine, Inc." },
    { "iPaper", "intelliPaper, LLC" },
    { "iqstor", "iQstor Networks, Inc." },
    { "iQue", "iQue" },
    { "ISi", "Information Storage inc." },
    { "Isilon", "Isilon Systems, Inc." },
    { "ISO", "International Standards Organization" },
    { "iStor", "iStor Networks, Inc." },
    { "ITC", "International Tapetronics Corporation" },
    { "iTwin", "iTwin Pte Ltd" },
    { "IVIVITY", "iVivity, Inc." },
    { "IVMMLTD", "InnoVISION Multimedia Ltd." },
    { "JABIL001", "Jabil Circuit" },
    { "JETWAY", "Jetway Information Co., Ltd" },
    { "JMR", "JMR Electronics Inc." },
    { "JOFEMAR", "Jofemar" },
    { "JOLLYLOG", "Jolly Logic" },
    { "JPC Inc.", "JPC Inc." },
    { "JSCSI", "jSCSI Project" },
    { "Juniper", "Juniper Networks" },
    { "JVC", "JVC Information Products Co." },
    { "KASHYA", "Kashya, Inc." },
    { "KENNEDY", "Kennedy Company" },
    { "KENWOOD", "KENWOOD Corporation" },
    { "KEWL", "Shanghai KEWL Imp&Exp Co., Ltd." },
    { "Key Tech", "Key Technologies, Inc" },
    { "KMNRIO", "Kaminario Technologies Ltd." },
    { "KODAK", "Eastman Kodak" },
    { "KONAN", "Konan" },
    { "koncepts", "koncepts International Ltd." },
    { "KONICA", "Konica Japan" },
    { "KOVE", "KOVE" },
    { "KSCOM", "KSCOM Co. Ltd.," },
    { "KUDELSKI", "Nagravision SA - Kudelski Group" },
    { "Kyocera", "Kyocera Corporation" },
    { "Lapida", "Gonmalo Electronics" },
    { "LAPINE", "Lapine Technology" },
    { "LASERDRV", "LaserDrive Limited" },
    { "LASERGR", "Lasergraphics, Inc." },
    { "LeapFrog", "LeapFrog Enterprises, Inc." },
    { "LEFTHAND", "LeftHand Networks (now HP)" },
    { "Leica", "Leica Camera AG" },
    { "Lexar", "Lexar Media, Inc." },
    { "LEYIO", "LEYIO" },
    { "LG", "LG Electronics Inc." },
    { "LGE", "LG Electronics Inc." },
    { "LIBNOVA", "LIBNOVA, SL Digital Preservation Systems" },
    { "LION", "Lion Optics Corporation" },
    { "LMS", "Laser Magnetic Storage International Company" },
    { "LoupTech", "Loup Technologies, Inc." },
    { "LSI", "LSI Corp. (was LSI Logic Corp.)" },
    { "LSILOGIC", "LSI Logic Storage Systems, Inc." },
    { "LTO-CVE", "Linear Tape - Open, Compliance Verification Entity" },
    { "LUXPRO", "Luxpro Corporation" },
    { "MacroSAN", "MacroSAN Technologies Co., Ltd." },
    { "Malakite", "Malachite Technologies (New VID is: Sandial)" },
    { "MarcBoon", "marcboon.com" },
    { "Marner", "Marner Storage Technologies, Inc." },
    { "MARVELL", "Marvell Semiconductor, Inc." },
    { "Matrix", "Matrix Orbital Corp." },
    { "MATSHITA", "Matsushita" },
    { "MAXELL", "Hitachi Maxell, Ltd." },
    { "MAXIM-IC", "Maxim Integrated Products" },
    { "MaxOptix", "Maxoptix Corp." },
    { "MAXSTRAT", "Maximum Strategy, Inc." },
    { "MAXTOR", "Maxtor Corp." },
    { "MaXXan", "MaXXan Systems, Inc." },
    { "MAYCOM", "maycom Co., Ltd." },
    { "MBEAT", "K-WON C&C Co.,Ltd" },
    { "MCC", "Measurement Computing Corporation" },
    { "McDATA", "McDATA Corporation" },
    { "MCUBE", "Mcube Technology Co., Ltd." },
    { "MDI", "Micro Design International, Inc." },
    { "MEADE", "Meade Instruments Corporation" },
    { "mediamat", "mediamatic" },
    { "MegaElec", "Mega Electronics Ltd" },
    { "MEII", "Mountain Engineering II, Inc." },
    { "MELA", "Mitsubishi Electronics America" },
    { "MELCO", "Mitsubishi Electric (Japan)" },
    { "mellanox", "Mellanox Technologies Ltd." },
    { "MEMOREX", "Memorex Telex Japan Ltd." },
    { "MEMREL", "Memrel Corporation" },
    { "MEMTECH", "MemTech Technology" },
    { "MendoCno", "Mendocino Software" },
    { "MERIDATA", "Oy Meridata Finland Ltd" },
    { "METHODEI", "Methode Electronics India pvt ltd" },
    { "METRUM", "Metrum, Inc." },
    { "MHTL", "Matsunichi Hi-Tech Limited" },
    { "MICROBTX", "Microbotics Inc." },
    { "Microchp", "Microchip Technology, Inc." },
    { "MICROLIT", "Microlite Corporation" },
    { "MICRON", "Micron Technology, Inc." },
    { "MICROP", "Micropolis" },
    { "MICROTEK", "Microtek Storage Corp" },
    { "Minitech", "Minitech (UK) Limited" },
    { "Minolta", "Minolta Corporation" },
    { "MINSCRIB", "Miniscribe" },
    { "MiraLink", "MiraLink Corporation" },
    { "Mirifica", "Mirifica s.r.l." },
    { "MITSUMI", "Mitsumi Electric Co., Ltd." },
    { "MKM", "Mitsubishi Kagaku Media Co., LTD." },
    { "Mobii", "Mobii Systems (Pty.) Ltd." },
    { "MOL", "Petrosoft Sdn. Bhd." },
    { "MOSAID", "Mosaid Technologies Inc." },
    { "MOTOROLA", "Motorola" },
    { "MP-400", "Daiwa Manufacturing Limited" },
    { "MPC", "MPC Corporation" },
    { "MPCCORP", "MPC Computers" },
    { "MPEYE", "Touchstone Technology Co., Ltd" },
    { "MPIO", "DKT Co.,Ltd" },
    { "MPM", "Mitsubishi Paper Mills, Ltd." },
    { "MPMan", "MPMan.com, Inc." },
    { "MSFT", "Microsoft Corporation" },
    { "MSI", "Micro-Star International Corp." },
    { "MST", "Morning Star Technologies, Inc." },
    { "MSystems", "M-Systems Flash Disk Pioneers" },
    { "MTI", "MTI Technology Corporation" },
    { "MTNGATE", "MountainGate Data Systems" },
    { "MXI", "Memory Experts International" },
    { "nac", "nac Image Technology Inc." },
    { "NAGRA", "Nagravision SA - Kudelski Group" },
    { "NAI", "North Atlantic Industries" },
    { "NAKAMICH", "Nakamichi Corporation" },
    { "NatInst", "National Instruments" },
    { "NatSemi", "National Semiconductor Corp." },
    { "NCITS", "InterNational Committee for Information Technology Standards (INCITS)" },
    { "NCL", "NCL America" },
    { "NCR", "NCR Corporation" },
    { "NDBTECH", "NDB Technologie Inc." },
    { "Neartek", "Neartek, Inc." },
    { "NEC", "NEC" },
    { "nemon", "NorthEast Monitoring" },
    { "NETAPP", "NetApp, Inc. (was Network Appliance)" },
    { "NetBSD", "The NetBSD Foundation" },
    { "Netcom", "Netcom Storage" },
    { "NETENGIN", "NetEngine, Inc." },
    { "NEWISYS", "Newisys Data Storage" },
    { "Newtech", "Newtech Co., Ltd." },
    { "NEXSAN", "Nexsan Technologies, Ltd." },
    { "NFINIDAT", "Infinidat Ltd." },
    { "NHR", "NH Research, Inc." },
    { "Nike", "Nike, Inc." },
    { "Nimble", "Nimble Storage" },
    { "NISCA", "NISCA Inc." },
    { "NISHAN", "Nishan Systems Inc." },
    { "Nitz", "Nitz Associates, Inc." },
    { "NKK", "NKK Corp." },
    { "NRC", "Nakamichi Research Corporation" },
    { "NSD", "Nippon Systems Development Co.,Ltd." },
    { "NSM", "NSM Jukebox GmbH" },
    { "nStor", "nStor Technologies, Inc." },
    { "NT", "Northern Telecom" },
    { "NUCONNEX", "NuConnex" },
    { "NUSPEED", "NuSpeed, Inc." },
    { "NVIDIA", "NVIDIA Corporation" },
    { "NVMe", "NVM Express Working Group" },
    { "OAI", "Optical Access International" },
    { "OCE", "Oce Graphics" },
    { "ODS", "ShenZhen DCS Group Co.,Ltd" },
    { "OHDEN", "Ohden Co., Ltd." },
    { "OKI", "OKI Electric Industry Co.,Ltd (Japan)" },
    { "Olidata", "Olidata S.p.A." },
    { "OMI", "Optical Media International" },
    { "OMNIFI", "Rockford Corporation - Omnifi Media" },
    { "OMNIS", "OMNIS Company (FRANCE)" },
    { "Ophidian", "Ophidian Designs" },
    { "opslag", "Tyrone Systems" },
    { "Optelec", "Optelec BV" },
    { "Optiarc", "Sony Optiarc Inc." },
    { "OPTIMEM", "Cipher/Optimem" },
    { "OPTOTECH", "Optotech" },
    { "ORACLE", "Oracle Corporation" },
    { "ORANGE", "Orange Micro, Inc." },
    { "ORCA", "Orca Technology" },
    { "Origin", "Origin Energy" },
    { "OSI", "Optical Storage International" },
    { "OSNEXUS", "OS NEXUS, Inc." },
    { "OTL", "OTL Engineering" },
    { "OVERLAND", "Overland Storage Inc." },
    { "pacdigit", "Pacific Digital Corp" },
    { "Packard", "Parkard Bell" },
    { "Panasas", "Panasas, Inc." },
    { "PARALAN", "Paralan Corporation" },
    { "PASCOsci", "Pasco Scientific" },
    { "PATHLGHT", "Pathlight Technology, Inc." },
    { "PCS", "Pro Charging Systems, LLC" },
    { "PerStor", "Perstor" },
    { "PERTEC", "Pertec Peripherals Corporation" },
    { "PFTI", "Performance Technology Inc." },
    { "PFU", "PFU Limited" },
    { "Phigment", "Phigment Technologies" },
    { "PHILIPS", "Philips Electronics" },
    { "PICO", "Packard Instrument Company" },
    { "PIK", "TECHNILIENT & MCS" },
    { "Pillar", "Pillar Data Systems" },
    { "PIONEER", "Pioneer Electronic Corp." },
    { "Pirus", "Pirus Networks" },
    { "PIVOT3", "Pivot3, Inc." },
    { "PLASMON", "Plasmon Data" },
    { "Pliant", "Pliant Technology, Inc." },
    { "PMCSIERA", "PMC-Sierra" },
    { "PME", "Precision Measurement Engineering" },
    { "PNNMed", "PNN Medical SA" },
    { "POKEN", "Poken SA" },
    { "POLYTRON", "PT. HARTONO ISTANA TEKNOLOGI" },
    { "PRAIRIE", "PrairieTek" },
    { "PREPRESS", "PrePRESS Solutions" },
    { "PRESOFT", "PreSoft Architects" },
    { "PRESTON", "Preston Scientific" },
    { "PRIAM", "Priam" },
    { "PRIMAGFX", "Primagraphics Ltd" },
    { "PRIMOS", "Primos" },
    { "PRNXDATA", "PernixData Inc." },
    { "PROCOM", "Procom Technology" },
    { "PROLIFIC", "Prolific Technology Inc." },
    { "ProMAX", "ProMAX Systems" },
    { "PROMISE", "PROMISE TECHNOLOGY, Inc" },
    { "PROSTOR", "ProStor Systems, Inc." },
    { "PROSUM", "PROSUM" },
    { "PROWARE", "Proware Technology Corp." },
    { "PTI", "Peripheral Technology Inc." },
    { "PTICO", "Pacific Technology International" },
    { "PULAX", "PULAX Corporation" },
    { "PURE", "PURE Storage" },
    { "Qi-Hardw", "Qi Hardware" },
    { "QIC", "Quarter-Inch Cartridge Drive Standards, Inc." },
    { "QLogic", "QLogic Corporation" },
    { "QNAP", "QNAP Systems" },
    { "Qsan", "QSAN Technology, Inc." },
    { "QStar", "QStar Technologies, Inc" },
    { "QUALSTAR", "Qualstar" },
    { "QUANTEL", "Quantel Ltd." },
    { "QUANTUM", "Quantum Corp." },
    { "QUEST", "Quest Software Inc." },
    { "QUIX", "Quix Computerware AG" },
    { "R-BYTE", "R-Byte, Inc." },
    { "RACALREC", "Racal Recorders" },
    { "RADITEC", "Radikal Technologies Deutschland GmbH" },
    { "RADSTONE", "Radstone Technology" },
    { "RAIDINC", "RAID Inc." },
    { "RASSYS", "Rasilient Systems Inc." },
    { "RASVIA", "Rasvia Systems, Inc." },
    { "rave-mp", "Go Video" },
    { "RDKMSTG", "MMS Dipl. Ing. Rolf-Dieter Klein" },
    { "RDStor", "Rorke China" },
    { "Readboy", "Readboy Ltd Co." },
    { "Realm", "Realm Systems" },
    { "realtek", "Realtek Semiconductor Corp." },
    { "REDUXIO", "Reduxio Systems Ltd." },
    { "rehanltd", "Rehan Electronics Ltd" },
    { "REKA", "REKA HEALTH PTE LTD" },
    { "RELDATA", "RELDATA Inc" },
    { "RENAGmbH", "RENA GmbH" },
    { "ReThinkM", "RETHINK MEDICAL, INC" },
    { "Revivio", "Revivio, Inc." },
    { "RGBLaser", "RGB Lasersysteme GmbH" },
    { "RGI", "Raster Graphics, Inc." },
    { "RHAPSODY", "Rhapsody Networks, Inc." },
    { "RHS", "Racal-Heim Systems GmbH" },
    { "RICOH", "Ricoh" },
    { "RODIME", "Rodime" },
    { "Rorke", "RD DATA Technology (ShenZhen) Limited" },
    { "Royaltek", "RoyalTek company Ltd." },
    { "RPS", "RPS" },
    { "RTI", "Reference Technology" },
    { "S-D", "Sauer-Danfoss" },
    { "S-flex", "Storageflex Inc" },
    { "S-SYSTEM", "S-SYSTEM" },
    { "S1", "storONE" },
    { "SAMSUNG", "Samsung Electronics Co., Ltd." },
    { "SAN", "Storage Area Networks, Ltd." },
    { "Sandial", "Sandial Systems, Inc." },
    { "SanDisk", "SanDisk Corporation" },
    { "SANKYO", "Sankyo Seiki" },
    { "SANRAD", "SANRAD Inc." },
    { "SANSSTOR", "SANS Technology, Inc.," },
    { "SANYO", "SANYO Electric Co., Ltd." },
    { "SC.Net", "StorageConnections.Net" },
    { "SCALE", "Scale Computing, Inc." },
    { "SCIENTEK", "SCIENTEK CORP" },
    { "SCInc.", "Storage Concepts, Inc." },
    { "SCREEN", "Dainippon Screen Mfg. Co., Ltd." },
    { "SDI", "Storage Dimensions, Inc." },
    { "SDS", "Solid Data Systems" },
    { "SEAC", "SeaChange International, Inc." },
    { "SEAGATE", "Seagate" },
    { "SEAGRAND", "SEAGRAND In Japan" },
    { "Seanodes", "Seanodes" },
    { "Sec. Key", "SecureKey Technologies Inc." },
    { "SEQUOIA", "Sequoia Advanced Technologies, Inc." },
    { "SGI", "Silicon Graphics International" },
    { "Shannon", "Shannon Systems Co., Ltd." },
    { "Shinko", "Shinko Electric Co., Ltd." },
    { "SIEMENS", "Siemens" },
    { "SigmaTel", "SigmaTel, Inc." },
    { "SII", "Seiko Instruments Inc." },
    { "SIMPLE", "SimpleTech, Inc. (Obs - now STEC, Inc.)" },
    { "SIVMSD", "IMAGO SOFTWARE SL" },
    { "SKhynix", "SK hynix Inc." },
    { "SKT", "SK Telecom Co., Ltd." },
    { "SLCNSTOR", "SiliconStor, Inc." },
    { "SLI", "Sierra Logic, Inc." },
    { "SMCI", "Super Micro Computer, Inc." },
    { "SmrtStor", "Smart Storage Systems" },
    { "SMS", "Scientific Micro Systems/OMTI" },
    { "SMSC", "SMSC Storage, Inc." },
    { "SMX", "Smartronix, Inc." },
    { "SNYSIDE", "Sunnyside Computing Inc." },
    { "SoftLock", "Softlock Digital Security Provider" },
    { "SolidFir", "SolidFire, Inc." },
    { "SONIC", "Sonic Solutions" },
    { "SoniqCas", "SoniqCast" },
    { "SONY", "Sony Corporation Japan" },
    { "SOUL", "Soul Storage Technology (Wuxi) Co., Ltd" },
    { "SPD", "Storage Products Distribution, Inc." },
    { "SPECIAL", "Special Computing Co." },
    { "SPECTRA", "Spectra Logic, a Division of Western Automation Labs, Inc." },
    { "SPERRY", "Sperry (now Unisys Corp.)" },
    { "Spintso", "Spintso International AB" },
    { "STARBORD", "Starboard Storage Systems, Inc." },
    { "STARWIND", "StarWind Software, Inc." },
    { "STEC", "STEC, Inc." },
    { "Sterling", "Sterling Diagnostic Imaging, Inc." },
    { "STK", "Storage Technology Corporation" },
    { "STNWOOD", "Stonewood Group" },
    { "STONEFLY", "StoneFly Networks, Inc." },
    { "STOR", "StorageNetworks, Inc." },
    { "STORAPP", "StorageApps, Inc. (now HP)" },
    { "STORCIUM", "Intelligent Systems Services Inc." },
    { "STORCOMP", "Storage Computer Corporation" },
    { "STORM", "Storm Technology, Inc." },
    { "StorMagc", "StorMagic" },
    { "Stratus", "Stratus Technologies" },
    { "StrmLgc", "StreamLogic Corp." },
    { "SUMITOMO", "Sumitomo Electric Industries, Ltd." },
    { "SUN", "Sun Microsystems, Inc." },
    { "SUNCORP", "SunCorporation" },
    { "suntx", "Suntx System Co., Ltd" },
    { "SUSE", "SUSE Linux" },
    { "Swinxs", "Swinxs BV" },
    { "SYMANTEC", "Symantec Corporation" },
    { "SYMBIOS", "Symbios Logic Inc." },
    { "SYMPLY", "Symply, Inc." },
    { "SYMWAVE", "Symwave, Inc." },
    { "SYNCSORT", "Syncsort Incorporated" },
    { "SYNERWAY", "Synerway" },
    { "SYNOLOGY", "Synology, Inc." },
    { "SyQuest", "SyQuest Technology, Inc." },
    { "SYSGEN", "Sysgen" },
    { "T-MITTON", "Transmitton England" },
    { "T-MOBILE", "T-Mobile USA, Inc." },
    { "T11", "INCITS Technical Committee T11" },
    { "TALARIS", "Talaris Systems, Inc." },
    { "TALLGRAS", "Tallgrass Technologies" },
    { "TANDBERG", "Tandberg Data A/S" },
    { "TANDEM", "Tandem (now HP)" },
    { "TANDON", "Tandon" },
    { "TCL", "TCL Shenzhen ASIC MIcro-electronics Ltd" },
    { "TDK", "TDK Corporation" },
    { "TEAC", "TEAC Japan" },
    { "TECOLOTE", "Tecolote Designs" },
    { "TEGRA", "Tegra Varityper" },
    { "Teilch", "Teilch" },
    { "Tek", "Tektronix" },
    { "TELLERT", "Tellert Elektronik GmbH" },
    { "TENTIME", "Laura Technologies, Inc." },
    { "TFDATACO", "TimeForge" },
    { "TGEGROUP", "TGE Group Co.,LTD." },
    { "Thecus", "Thecus Technology Corp." },
    { "TI-DSG", "Texas Instruments" },
    { "TiGi", "TiGi Corporation" },
    { "TILDESGN", "Tildesign bv" },
    { "TINTRI", "Tintri" },
    { "Tite", "Tite Technology Limited" },
    { "TKS Inc.", "TimeKeeping Systems, Inc." },
    { "TLMKS", "Telemakus LLC" },
    { "TMS", "Texas Memory Systems, Inc." },
    { "TMS100", "TechnoVas" },
    { "TOLISGRP", "The TOLIS Group" },
    { "TOSHIBA", "Toshiba Japan" },
    { "TOYOU", "TOYOU FEIJI ELECTRONICS CO.,LTD." },
    { "Tracker", "Tracker, LLC" },
    { "TRIOFLEX", "Trioflex Oy" },
    { "TRIPACE", "Tripace" },
    { "TRLogger", "TrueLogger Ltd." },
    { "TROIKA", "Troika Networks, Inc." },
    { "TRULY", "TRULY Electronics MFG. LTD." },
    { "TRUSTED", "Trusted Data Corporation" },
    { "TSSTcorp", "Toshiba Samsung Storage Technology Corporation" },
    { "TZM", "TZ Medical" },
    { "UD-DVR", "Bigstone Project." },
    { "UDIGITAL", "United Digital Limited" },
    { "UIT", "United Infomation Technology" },
    { "ULTRA", "UltraStor Corporation" },
    { "UNISTOR", "Unistor Networks, Inc." },
    { "UNISYS", "Unisys" },
    { "USCORE", "Underscore, Inc." },
    { "USDC", "US Design Corp." },
    { "VASCO", "Vasco Data Security" },
    { "VDS", "Victor Data Systems Co., Ltd." },
    { "VELDANA", "VELDANA MEDICAL SA" },
    { "VENTANA", "Ventana Medical Systems" },
    { "Verari", "Verari Systems, Inc." },
    { "VERBATIM", "Verbatim Corporation" },
    { "Vercet", "Vercet LLC" },
    { "VERITAS", "VERITAS Software Corporation" },
    { "Vexata", "Vexata Inc" },
    { "VEXCEL", "VEXCEL IMAGING GmbH" },
    { "VicomSys", "Vicom Systems, Inc." },
    { "VIDEXINC", "Videx, Inc." },
    { "VIOLIN", "Violin Memory, Inc." },
    { "VIRIDENT", "Virident Systems, Inc." },
    { "VITESSE", "Vitesse Semiconductor Corporation" },
    { "VIXEL", "Vixel Corporation (now part of Emulex)" },
    { "VLS", "Van Lent Systems BV" },
    { "VMAX", "VMAX Technologies Corp." },
    { "VMware", "VMware Inc." },
    { "Vobis", "Vobis Microcomputer AG" },
    { "VOLTAIRE", "Voltaire Ltd." },
    { "VRC", "Vermont Research Corp." },
    { "VRugged", "Vanguard Rugged Storage" },
    { "VTGadget", "Vermont Gadget Company" },
    { "Waitec", "Waitec NV" },
    { "WangDAT", "WangDAT" },
    { "WANGTEK", "Wangtek" },
    { "Wasabi", "Wasabi Systems" },
    { "WAVECOM", "Wavecom" },
    { "WD", "Western Digital Corporation" },
    { "WDC", "Western Digital Corporation" },
    { "WDIGTL", "Western Digital" },
    { "WDTI", "Western Digital Technologies, Inc." },
    { "WEARNES", "Wearnes Technology Corporation" },
    { "WeeraRes", "Weera Research Pte Ltd" },
    { "Wildflwr", "Wildflower Technologies, Inc." },
    { "WSC0001", "Wisecom, Inc." },
    { "X3", "InterNational Committee for Information Technology Standards (INCITS)" },
    { "XEBEC", "Xebec Corporation" },
    { "XENSRC", "XenSource, Inc." },
    { "Xerox", "Xerox Corporation" },
    { "Xield", "Xield Technologies Limited" },
    { "XIOtech", "XIOtech Corporation" },
    { "XIRANET", "Xiranet Communications GmbH" },
    { "XIV", "XIV (now IBM)" },
    { "XtremIO", "XtremIO" },
    { "XYRATEX", "Xyratex" },
    { "YINHE", "NUDT Computer Co." },
    { "YIXUN", "Yixun Electronic Co.,Ltd." },
    { "YOTTA", "YottaYotta, Inc." },
    { "Zarva", "Zarva Digital Technology Co., Ltd." },
    { "ZBS", "SMARTX Corporation" },
    { "ZETTA", "Zetta Systems, Inc." },
    { "ZTE", "ZTE Corporation" },
    { "ZVAULT", "Zetavault" }
};

const ScsiVendor kScsiVendorsAlt[] =
{
    { "ZMVE", "Zalman Tech Co." }
};

void AddProfileMedia(proto::OpticalDrive::Media* media, DeviceSPTI& device,uint16_t profile)
{
    // FIXME: DVD-RW Double Layer write support not handled.

    constexpr uint32_t kRead = proto::OpticalDrive::Media::MODE_READ;
    constexpr uint32_t kWrite = proto::OpticalDrive::Media::MODE_WRITE;

    switch (profile)
    {
        case DeviceSPTI::PROFILE_CD_ROM:
        {
            media->set_cd_rom(media->cd_rom() | kRead);
            media->set_cd_r(media->cd_r() | kRead);
            media->set_cd_rw(media->cd_rw() | kRead);
        }
        break;

        case DeviceSPTI::PROFILE_CD_R:
        {
            media->set_cd_r(media->cd_r() | kWrite);
        }
        break;

        case DeviceSPTI::PROFILE_CD_RW:
        {
            media->set_cd_rw(media->cd_rw() | kWrite);
        }
        break;

        case DeviceSPTI::PROFILE_DVD_ROM:
        {
            media->set_dvd_rom(media->dvd_rom() | kRead);
            media->set_dvd_r(media->dvd_r() | kRead);
            media->set_dvd_rw(media->dvd_rw() | kRead);

            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_READ, &feature))
            {
                if (feature.table.dvd_read.dual_r != 0)
                    media->set_dvd_r_dl(media->dvd_r_dl() | kRead);

                if (feature.table.dvd_read.dual_rw != 0)
                    media->set_dvd_rw_dl(media->dvd_rw_dl() | kRead);
            }
        }
        break;

        case DeviceSPTI::PROFILE_DVD_R_SEQUENTIAL_RECORDING:
        {
            media->set_dvd_r(media->dvd_r() | kWrite);
        }
        break;

        case DeviceSPTI::PROFILE_DVD_RAM:
        {
            media->set_dvd_ram(kRead | kWrite);
        }
        break;

        case DeviceSPTI::PROFILE_DVD_RW_SEQUENTIAL_RECORDING:
        {
            media->set_dvd_rw(media->dvd_rw() | kWrite);
        }
        break;

        case DeviceSPTI::PROFILE_DVD_R_DUAL_LAYER_SEQUENTIAL_RECORDING:
        case DeviceSPTI::PROFILE_DVD_R_DUAL_LAYER_JUMP_RECORDING:
        {
            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_R_RW_WRITE, &feature))
            {
                if (feature.table.dvd_r_rw_write.rdl != 0)
                    media->set_dvd_r_dl(media->dvd_r_dl() | kWrite);
            }
        }
        break;

        case DeviceSPTI::PROFILE_DVD_PLUS_RW:
        {
            media->set_dvd_plus_rw(media->dvd_plus_rw() | kRead);

            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_PLUS_RW, &feature))
            {
                if (feature.table.dvd_plus_rw.write != 0)
                    media->set_dvd_plus_rw(media->dvd_plus_rw() | kWrite);
            }
        }
        break;

        case DeviceSPTI::PROFILE_DVD_PLUS_R:
        {
            media->set_dvd_plus_r(media->dvd_plus_r() | kRead);

            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_PLUS_R, &feature))
            {
                if (feature.table.dvd_plus_r.write != 0)
                    media->set_dvd_plus_r(media->dvd_plus_r() | kWrite);
            }
        }
        break;

        case DeviceSPTI::PROFILE_DVD_PLUS_RW_DUAL_LAYER:
        {
            media->set_dvd_plus_rw_dl(media->dvd_plus_rw_dl() | kRead);

            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_PLUS_RW_DUAL_LAYER, &feature))
            {
                if (feature.table.dvd_plus_rw_dual_layer.write != 0)
                    media->set_dvd_plus_rw_dl(media->dvd_plus_rw_dl() | kWrite);
            }
        }
        break;

        case DeviceSPTI::PROFILE_DVD_PLUS_R_DUAL_LAYER:
        {
            media->set_dvd_plus_r_dl(media->dvd_plus_r_dl() | kRead);

            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_PLUS_R_DUAL_LAYER, &feature))
            {
                if (feature.table.dvd_plus_r_dual_layer.write != 0)
                    media->set_dvd_plus_r_dl(media->dvd_plus_r_dl() | kWrite);
            }
        }
        break;

        case DeviceSPTI::PROFILE_BD_ROM:
        {
            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_BD_READ, &feature))
            {
                if (ByteSwap(feature.table.bd_read.rom) != 0)
                    media->set_bd_rom(media->bd_rom() | kRead);

                if (ByteSwap(feature.table.bd_read.r) != 0)
                    media->set_bd_r(media->bd_r() | kRead);

                if (ByteSwap(feature.table.bd_read.re) != 0)
                    media->set_bd_re(media->bd_re() | kRead);
            }
        }
        break;

        case DeviceSPTI::PROFILE_BD_RE:
        case DeviceSPTI::PROFILE_BD_R:
        {
            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_BD_WRITE, &feature))
            {
                if (ByteSwap(feature.table.bd_write.re) != 0)
                    media->set_bd_re(media->bd_re() | kWrite);

                if (ByteSwap(feature.table.bd_write.r) != 0)
                    media->set_bd_r(media->bd_r() | kWrite);
            }
        }
        break;

        case DeviceSPTI::PROFILE_HD_DVD_ROM:
        {
            media->set_hd_dvd_rom(media->hd_dvd_rom() | kRead);

            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_HD_DVD_READ, &feature))
            {
                if (feature.table.hd_dvd_read.hd_dvd_r != 0)
                    media->set_hd_dvd_r(media->hd_dvd_r() | kRead);

                if (feature.table.hd_dvd_read.hd_dvd_ram != 0)
                    media->set_hd_dvd_ram(media->hd_dvd_ram() | kRead);
            }
        }
        break;

        case DeviceSPTI::PROFILE_HD_DVD_R:
        case DeviceSPTI::PROFILE_HD_DVD_RAM:
        {
            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_HD_DVD_WRITE, &feature))
            {
                if (feature.table.hd_dvd_write.hd_dvd_r != 0)
                    media->set_hd_dvd_r(media->hd_dvd_r() | kWrite);

                if (feature.table.hd_dvd_write.hd_dvd_ram != 0)
                    media->set_hd_dvd_ram(media->hd_dvd_ram() | kWrite);
            }
        }
        break;
    }
}

void AddProfileFeatures(
    proto::OpticalDrive::Features* features, DeviceSPTI& device, uint16_t profile)
{
    switch (profile)
    {
        case DeviceSPTI::PROFILE_CD_ROM:
        {
            DeviceSPTI::Feature feature;
            memset(&feature, 0, sizeof(feature));

            if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_CD_READ, &feature))
            {
                features->set_has_cd_text(feature.table.cd_read.cd_text != 0);
                features->set_has_c2_error_pointers(feature.table.cd_read.c2_flags != 0);
            }
        }
        break;

        case DeviceSPTI::PROFILE_DVD_DOWNLOAD_DISK_RECORDING:
        {
            features->set_has_dvd_download_disk_recording(true);
        }
        break;
    }
}

void AddFeatures(proto::OpticalDrive::Features* features, DeviceSPTI& device)
{
    DeviceSPTI::Feature feature;
    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_AACS, &feature))
    {
        features->set_has_aacs(ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_AACS);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_VCPS, &feature))
    {
        features->set_has_vcps(ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_VCPS);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_SMART, &feature))
    {
        features->set_has_smart(ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_SMART);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_CPRM, &feature))
    {
        features->set_has_cprm(
            ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_DVD_CPRM);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_DVD_CSS, &feature))
    {
        features->set_has_css(ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_DVD_CSS);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_POWER_MANAGEMENT, &feature))
    {
        features->set_has_power_management(
            ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_POWER_MANAGEMENT);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_HYBRID_DISK, &feature))
    {
        features->set_has_hybrid_disk(
            ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_HYBRID_DISK);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_LAYER_JUMP_RECORDING, &feature))
    {
        features->set_has_layer_jump_recording(
            ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_LAYER_JUMP_RECORDING);
    }

    memset(&feature, 0, sizeof(feature));

    if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_MRW, &feature))
    {
        features->set_has_mount_rainier(
            ByteSwap(feature.feature_code) == DeviceSPTI::FEATURE_CODE_MRW);
    }
}

const char* InterfaceToString(proto::OpticalDrive::InterfaceType value)
{
    switch (value)
    {
        case proto::OpticalDrive::INTERFACE_TYPE_UNSPECIFIED:
            return "Unspecified";

        case proto::OpticalDrive::INTERFACE_TYPE_SCSI:
            return "SCSI";

        case proto::OpticalDrive::INTERFACE_TYPE_ATAPI:
            return "ATAPI";

        case proto::OpticalDrive::INTERFACE_TYPE_IEEE1394_1995:
            return "IEEE 1394 (1995)";

        case proto::OpticalDrive::INTERFACE_TYPE_IEEE1394A:
            return "IEEE 1394A";

        case proto::OpticalDrive::INTERFACE_TYPE_FIBRE_CHANNEL:
            return "Fibre Channel";

        case proto::OpticalDrive::INTERFACE_TYPE_IEEE1394B:
            return "IEEE 1394B";

        case proto::OpticalDrive::INTERFACE_TYPE_SERIAL_ATAPI:
            return "Serial ATAPI";

        case proto::OpticalDrive::INTERFACE_TYPE_USB:
            return "USB";

        default:
            return "Unknown";
    }
}

const char* VendorCodeToName(const char* vendor_code)
{
    for (size_t i = 0; i < _countof(kScsiVendors); ++i)
    {
        if (_stricmp(kScsiVendors[i].vendor_code, vendor_code) == 0)
            return kScsiVendors[i].vendor_name;
    }

    for (size_t i = 0; i < _countof(kScsiVendorsAlt); ++i)
    {
        if (_stricmp(kScsiVendorsAlt[i].vendor_code, vendor_code) == 0)
            return kScsiVendorsAlt[i].vendor_name;
    }

    return "Unknown";
}

} // namespace

CategoryOpticalDrive::CategoryOpticalDrive()
    : CategoryInfo(Type::INFO_PARAM_VALUE)
{
    // Nothing
}

const char* CategoryOpticalDrive::Name() const
{
    return "Optical Drives";
}

Category::IconId CategoryOpticalDrive::Icon() const
{
    return IDI_DRIVE_DISK;
}

const char* CategoryOpticalDrive::Guid() const
{
    return "{68E028FE-3DA6-4BAF-9E18-CDB828372860}";
}

void CategoryOpticalDrive::Parse(Table& table, const std::string& data)
{
    proto::OpticalDrive message;

    if (!message.ParseFromString(data))
        return;

    table.AddColumns(ColumnList::Create()
                     .AddColumn("Parameter", 300)
                     .AddColumn("Value", 260));

    for (int index = 0; index < message.item_size(); ++index)
    {
        const proto::OpticalDrive::Item& item = message.item(index);

        Group group = table.AddGroup(item.device_name());

        group.AddParam("Vendor Code", Value::String(item.vendor_code()));
        group.AddParam("Vendor Name", Value::String(VendorCodeToName(item.vendor_code().c_str())));
        group.AddParam("Firmware Revision", Value::String(item.firmware_version()));
        group.AddParam("Interface", Value::String(InterfaceToString(item.interface_type())));
        group.AddParam("Buffer Size", Value::MemorySize(item.buffer_size()));

        group.AddParam("Region Code", item.region_code() != 0 ?
                       Value::Number(item.region_code()) : Value::String("No"));

        group.AddParam("Remaining User Changes", Value::Number(item.region_code_user_changes()));
        group.AddParam("Remaining Vendor Changes", Value::Number(item.region_code_vendor_changes()));

        {
            const proto::OpticalDrive::Media& media = item.media();

            Group media_group = group.AddGroup("Supported Disk Types");

            auto add_type = [&](const char* type_name, uint32_t media_flags)
            {
                const char* value = "Not Supported";

                if ((media_flags & proto::OpticalDrive::Media::MODE_READ) &&
                    (media_flags & proto::OpticalDrive::Media::MODE_WRITE))
                {
                    value = "Read + Write";
                }
                else if (media_flags & proto::OpticalDrive::Media::MODE_READ)
                {
                    value = "Read";
                }

                media_group.AddParam(type_name, Value::String(value));
            };

            add_type("BD-ROM", media.bd_rom());
            add_type("BD-R", media.bd_r());
            add_type("BD-RE", media.bd_re());
            add_type("HD DVD-ROM", media.hd_dvd_rom());
            add_type("HD DVD-R", media.hd_dvd_r());
            add_type("HD DVD-RAM", media.hd_dvd_ram());
            add_type("DVD-ROM", media.dvd_rom());
            add_type("DVD+R Dual Layer", media.dvd_plus_r_dl());
            add_type("DVD+RW Dual Layer", media.dvd_plus_rw_dl());
            add_type("DVD+R", media.dvd_plus_r());
            add_type("DVD+RW", media.dvd_plus_rw());
            add_type("DVD-R Dual Layer", media.dvd_r_dl());
            add_type("DVD-RW Dual Layer", media.dvd_rw_dl());
            add_type("DVD-R", media.dvd_r());
            add_type("DVD-RW", media.dvd_rw());
            add_type("DVD-RAM", media.dvd_ram());
            add_type("CD-ROM", media.cd_rom());
            add_type("CD-R", media.cd_r());
            add_type("CD-RW", media.cd_rw());
        }

        {
            const proto::OpticalDrive::Features& features = item.features();

            Group features_group = group.AddGroup("Features");

            group.AddParam("C2 Error Pointers", Value::Bool(features.has_c2_error_pointers()));
            group.AddParam("AACS", Value::Bool(features.has_aacs()));
            group.AddParam("CD-Text", Value::Bool(features.has_cd_text()));
            group.AddParam("CPRM", Value::Bool(features.has_cprm()));
            group.AddParam("CSS", Value::Bool(features.has_css()));
            group.AddParam("DVD-Download Disk Recording", Value::Bool(features.has_dvd_download_disk_recording()));
            group.AddParam("Hybrid Disk", Value::Bool(features.has_hybrid_disk()));
            group.AddParam("Layer Jump Recording", Value::Bool(features.has_layer_jump_recording()));
            group.AddParam("Mount Rainier", Value::Bool(features.has_mount_rainier()));
            group.AddParam("Power Management", Value::Bool(features.has_power_management()));
            group.AddParam("SMART", Value::Bool(features.has_smart()));
            group.AddParam("VCPS", Value::Bool(features.has_vcps()));
        }
    }
}

std::string CategoryOpticalDrive::Serialize()
{
    proto::OpticalDrive message;

    LogicalDriveEnumerator enumerator;

    for (;;)
    {
        std::experimental::filesystem::path path = enumerator.Next();

        if (path.empty())
            break;

        if (enumerator.GetInfo().Type() != LogicalDriveEnumerator::DriveInfo::DriveType::CDROM)
            continue;

        std::wstring device_path = StringPrintf(L"\\\\.\\%c:", path.c_str()[0]);
        DeviceSPTI device;

        if (!device.Open(device_path))
        {
            DPLOG(LS_WARNING) << "device.Open(" << device_path << ") failed";
            continue;
        }

        DeviceSPTI::InquiryData inquiry_data;
        memset(&inquiry_data, 0, sizeof(inquiry_data));

        if (!device.GetInquiryData(&inquiry_data))
        {
            DPLOG(LS_WARNING) << "device.Inquiry failed";
            continue;
        }

        char vendor[8 + 1] = { 0 };
        memcpy(vendor, inquiry_data.vendor_id, 8);

        char product_id[16 + 1] = { 0 };
        memcpy(product_id, inquiry_data.product_id, 16);

        char revision[4 + 1] = { 0 };
        memcpy(revision, inquiry_data.product_revision_level, 4);

        proto::OpticalDrive::Item* item = message.add_item();

        TrimWhitespaceASCII(vendor, TrimPositions::TRIM_ALL, *item->mutable_vendor_code());
        TrimWhitespaceASCII(product_id, TrimPositions::TRIM_ALL, *item->mutable_device_name());
        TrimWhitespaceASCII(revision, TrimPositions::TRIM_ALL, *item->mutable_firmware_version());

        DeviceSPTI::Feature feature;
        memset(&feature, 0, sizeof(feature));

        if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_CORE, &feature))
        {
            switch (ByteSwap(feature.table.core.physical_interface_standard))
            {
                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_UNSPECIFIED:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_UNSPECIFIED);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_SCSI_FAMILY:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_SCSI);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_ATAPI:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_ATAPI);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_IEEE_1394_1995:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_IEEE1394_1995);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_IEEE_1394A:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_IEEE1394A);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_FIBRE_CHANNEL:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_FIBRE_CHANNEL);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_IEEE_1394B:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_IEEE1394B);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_SERIAL_ATAPI:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_SERIAL_ATAPI);
                    break;

                case DeviceSPTI::PHYSICAL_INTERFACE_STANDARD_USB:
                    item->set_interface_type(proto::OpticalDrive::INTERFACE_TYPE_USB);
                    break;
            }
        }

        memset(&feature, 0, sizeof(feature));

        if (device.GetConfiguration(DeviceSPTI::FEATURE_CODE_PROFILE_LIST, &feature))
        {
            for (int i = 0; i < feature.additional_length; i += 4)
            {
                uint16_t profile = ByteSwap(*(uint16_t*)(&feature.table.additional_data[i]));

                AddProfileMedia(item->mutable_media(), device, profile);
                AddProfileFeatures(item->mutable_features(), device, profile);
            }
        }

        AddFeatures(item->mutable_features(), device);

        DeviceSPTI::ReportKeyData report_key_data;
        memset(&report_key_data, 0, sizeof(report_key_data));

        if (device.GetReportKeyData(&report_key_data))
        {
            item->set_region_code(report_key_data.type_code);
            item->set_region_code_user_changes(report_key_data.user_changes);
            item->set_region_code_vendor_changes(report_key_data.vendor_resets);
        }

        DeviceSPTI::ModeSenseData mode_sense;
        memset(&mode_sense, 0, sizeof(mode_sense));

        if (device.GetModeSenseData(DeviceSPTI::MODE_SENSE_PAGE_2A, &mode_sense))
        {
            item->set_buffer_size(ByteSwap(mode_sense.page.page_2A.buffer_size) * 1024);
        }
    }

    return message.SerializeAsString();
}

} // namespace aspia
