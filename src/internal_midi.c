/*
 * internal_midi.c -- Midi Wavetable Processing library
 *
 * Copyright (C) WildMIDI Developers 2001-2016
 *
 * This file is part of WildMIDI.
 *
 * WildMIDI is free software: you can redistribute and/or modify the player
 * under the terms of the GNU General Public License and you can redistribute
 * and/or modify the library under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either version 3 of
 * the licenses, or(at your option) any later version.
 *
 * WildMIDI is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
 * the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License and the
 * GNU Lesser General Public License along with WildMIDI.  If not,  see
 * <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "common.h"
#include "lock.h"
#include "wm_error.h"
#include "reverb.h"
#include "sample.h"
#include "wildmidi_lib.h"
#include "patches.h"
#include "internal_midi.h"
#ifdef __DJGPP__
#define powf pow /* prefer C89 pow() from libc.a instead of powf() from libm. */
#endif
#ifdef WILDMIDI_AMIGA
#define powf pow
#endif
#if defined(__WATCOMC__) || defined(__EMX__)
#define powf pow
#endif

#define HOLD_OFF 0x02

//#define DEBUG_MIDI

#ifdef DEBUG_MIDI
#define MIDI_EVENT_DEBUG(dx,dy,dz) fprintf(stderr,"\r%s, 0x%.2x, 0x%.8x\n",dx,dy,dz)
#define MIDI_EVENT_SDEBUG(dx,dy,dz) fprintf(stderr,"\r%s, 0x%.2x, %s\n",dx,dy,dz)
#else
#define MIDI_EVENT_DEBUG(dx,dy,dz)
#define MIDI_EVENT_SDEBUG(dx,dy,dz)
#endif

/* f: ( VOLUME / 127.0 ) * 1024.0 */
int16_t _WM_lin_volume[] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 72, 80, 88, 96,
    104, 112, 120, 129, 137, 145, 153, 161, 169, 177, 185, 193, 201, 209, 217,
    225, 233, 241, 249, 258, 266, 274, 282, 290, 298, 306, 314, 322, 330, 338,
    346, 354, 362, 370, 378, 387, 395, 403, 411, 419, 427, 435, 443, 451, 459,
    467, 475, 483, 491, 499, 507, 516, 524, 532, 540, 548, 556, 564, 572, 580,
    588, 596, 604, 612, 620, 628, 636, 645, 653, 661, 669, 677, 685, 693, 701,
    709, 717, 725, 733, 741, 749, 757, 765, 774, 782, 790, 798, 806, 814, 822,
    830, 838, 846, 854, 862, 870, 878, 886, 894, 903, 911, 919, 927, 935, 943,
    951, 959, 967, 975, 983, 991, 999, 1007, 1015, 1024 };

/* f: As per midi 2 standard */
static float dBm_volume[] = { -999999.999999f, -84.15214884f, -72.11094901f,
    -65.06729865f, -60.06974919f, -56.19334866f, -53.02609882f, -50.34822724f,
    -48.02854936f, -45.98244846f, -44.15214884f, -42.49644143f, -40.984899f,
    -39.59441475f, -38.30702741f, -37.10849848f, -35.98734953f, -34.93419198f,
    -33.94124863f, -33.0020048f, -32.11094901f, -31.26337705f, -30.45524161f,
    -29.6830354f, -28.94369917f, -28.23454849f, -27.55321492f, -26.89759827f,
    -26.26582758f, -25.65622892f, -25.06729865f, -24.49768108f, -23.94614971f,
    -23.41159124f, -22.89299216f, -22.38942706f, -21.90004881f, -21.42407988f,
    -20.96080497f, -20.50956456f, -20.06974919f, -19.64079457f, -19.22217722f,
    -18.81341062f, -18.41404178f, -18.02364829f, -17.64183557f, -17.26823452f,
    -16.90249934f, -16.54430564f, -16.19334866f, -15.84934179f, -15.51201509f,
    -15.18111405f, -14.85639845f, -14.53764126f, -14.22462776f, -13.91715461f,
    -13.6150291f, -13.31806837f, -13.02609882f, -12.73895544f, -12.45648126f,
    -12.17852686f, -11.90494988f, -11.63561457f, -11.37039142f, -11.10915673f,
    -10.85179233f, -10.59818521f, -10.34822724f, -10.10181489f, -9.858848981f,
    -9.619234433f, -9.382880049f, -9.149698303f, -8.919605147f, -8.692519831f,
    -8.468364731f, -8.247065187f, -8.028549359f, -7.812748083f, -7.599594743f,
    -7.389025143f, -7.180977396f, -6.97539181f, -6.772210788f, -6.571378733f,
    -6.372841952f, -6.176548572f, -5.982448461f, -5.790493145f, -5.600635744f,
    -5.412830896f, -5.227034694f, -5.043204627f, -4.861299517f, -4.681279468f,
    -4.503105811f, -4.326741054f, -4.152148838f, -3.979293887f, -3.808141968f,
    -3.63865985f, -3.470815266f, -3.304576875f, -3.139914228f, -2.976797731f,
    -2.815198619f, -2.655088921f, -2.496441432f, -2.339229687f, -2.183427931f,
    -2.029011099f, -1.875954785f, -1.724235224f, -1.573829269f, -1.424714368f,
    -1.276868546f, -1.130270383f, -0.9848989963f, -0.8407340256f, -0.6977556112f,
    -0.5559443807f, -0.4152814317f, -0.2757483179f, -0.1373270335f, 0.0f };

/* f: As per midi 2 standard */
static float dBm_pan_volume[] = { -999999.999999f, -38.15389834f, -32.13396282f,
    -28.61324502f, -26.1160207f, -24.179814f, -22.5986259f, -21.26257033f,
    -20.10605521f, -19.08677237f, -18.17583419f, -17.35263639f, -16.60196565f,
    -15.91226889f, -15.2745658f, -14.6817375f, -14.12804519f, -13.60879499f,
    -13.12009875f, -12.65869962f, -12.22184237f, -11.80717543f, -11.41267571f,
    -11.03659017f, -10.67738981f, -10.33373306f, -10.00443638f, -9.6884504f,
    -9.384840381f, -9.092770127f, -8.811488624f, -8.540318866f, -8.278648457f,
    -8.025921658f, -7.781632628f, -7.545319633f, -7.316560087f, -7.094966257f,
    -6.880181552f, -6.671877289f, -6.46974987f, -6.273518306f, -6.082922045f,
    -5.897719045f, -5.717684082f, -5.542607236f, -5.372292553f, -5.206556845f,
    -5.045228616f, -4.888147106f, -4.735161423f, -4.586129765f, -4.44091872f,
    -4.299402626f, -4.161462998f, -4.026988004f, -3.895871989f, -3.76801504f,
    -3.643322591f, -3.52170506f, -3.403077519f, -3.287359388f, -3.174474158f,
    -3.064349129f, -2.956915181f, -2.852106549f, -2.749860626f, -2.650117773f,
    -2.55282115f, -2.457916557f, -2.36535228f, -2.27507896f, -2.187049463f,
    -2.101218759f, -2.017543814f, -1.935983486f,-1.856498429f, -1.779051001f,
    -1.703605184f, -1.630126502f, -1.558581949f, -1.48893992f, -1.421170148f,
    -1.35524364f, -1.291132623f, -1.228810491f, -1.168251755f, -1.109431992f,
    -1.052327808f, -0.9969167902f, -0.9431774708f, -0.8910892898f, -0.8406325604f,
    -0.7917884361f, -0.7445388804f, -0.6988666373f, -0.6547552046f, -0.612188808f,
    -0.5711523768f, -0.5316315211f, -0.4936125107f, -0.4570822543f, -0.4220282808f,
    -0.3884387214f, -0.3563022927f, -0.3256082808f, -0.2963465264f, -0.2685074109f,
    -0.2420818435f, -0.2170612483f, -0.1934375538f, -0.1712031815f, -0.1503510361f,
    -0.1308744964f, -0.1127674066f, -0.09602406855f, -0.08063923423f,
    -0.06660809932f, -0.05392629701f, -0.04258989258f, -0.03259537844f,
    -0.02393966977f, -0.01662010072f, -0.01063442111f, -0.005980793601f,
    -0.002657791522f, -0.000664397052f, 0.0f };

uint32_t _WM_freq_table[] = { 837201792, 837685632, 838169728,
    838653568, 839138240, 839623232, 840108480, 840593984, 841079680,
    841565184, 842051648, 842538240, 843025152, 843512320, 843999232,
    844486976, 844975040, 845463360, 845951936, 846440320, 846929536,
    847418944, 847908608, 848398656, 848888960, 849378944, 849869824,
    850361024, 850852416, 851344192, 851835584, 852327872, 852820480,
    853313280, 853806464, 854299328, 854793024, 855287040, 855781312,
    856275904, 856770752, 857265344, 857760704, 858256448, 858752448,
    859248704, 859744768, 860241600, 860738752, 861236160, 861733888,
    862231360, 862729600, 863228160, 863727104, 864226176, 864725696,
    865224896, 865724864, 866225152, 866725760, 867226688, 867727296,
    868228736, 868730496, 869232576, 869734912, 870236928, 870739904,
    871243072, 871746560, 872250368, 872754496, 873258240, 873762880,
    874267840, 874773184, 875278720, 875783936, 876290112, 876796480,
    877303232, 877810176, 878317504, 878824512, 879332416, 879840576,
    880349056, 880857792, 881366272, 881875712, 882385280, 882895296,
    883405440, 883915456, 884426304, 884937408, 885448832, 885960512,
    886472512, 886984192, 887496768, 888009728, 888522944, 889036352,
    889549632, 890063680, 890578048, 891092736, 891607680, 892122368,
    892637952, 893153792, 893670016, 894186496, 894703232, 895219648,
    895737024, 896254720, 896772672, 897290880, 897808896, 898327744,
    898846912, 899366336, 899886144, 900405568, 900925952, 901446592,
    901967552, 902488768, 903010368, 903531584, 904053760, 904576256,
    905099008, 905622016, 906144896, 906668480, 907192512, 907716800,
    908241408, 908765632, 909290816, 909816256, 910342144, 910868160,
    911394624, 911920768, 912447680, 912975104, 913502720, 914030592,
    914558208, 915086784, 915615552, 916144768, 916674176, 917203968,
    917733440, 918263744, 918794496, 919325440, 919856704, 920387712,
    920919616, 921451840, 921984320, 922517184, 923049728, 923583168,
    924116928, 924651008, 925185344, 925720000, 926254336, 926789696,
    927325312, 927861120, 928397440, 928933376, 929470208, 930007296,
    930544768, 931082560, 931619968, 932158464, 932697152, 933236160,
    933775488, 934315072, 934854464, 935394688, 935935296, 936476224,
    937017344, 937558208, 938100160, 938642304, 939184640, 939727488,
    940269888, 940813312, 941357056, 941900992, 942445440, 942990016,
    943534400, 944079680, 944625280, 945171200, 945717440, 946263360,
    946810176, 947357376, 947904832, 948452672, 949000192, 949548608,
    950097280, 950646400, 951195776, 951745472, 952294912, 952845184,
    953395904, 953946880, 954498176, 955049216, 955601088, 956153408,
    956705920, 957258816, 957812032, 958364928, 958918848, 959472960,
    960027456, 960582272, 961136768, 961692224, 962248000, 962804032,
    963360448, 963916608, 964473600, 965031040, 965588736, 966146816,
    966705152, 967263168, 967822144, 968381440, 968941120, 969501056,
    970060736, 970621376, 971182272, 971743488, 972305088, 972866368,
    973428608, 973991104, 974554048, 975117312, 975680768, 976243968,
    976808192, 977372736, 977937536, 978502656, 979067584, 979633344,
    980199488, 980765888, 981332736, 981899200, 982466688, 983034432,
    983602624, 984171008, 984739776, 985308160, 985877632, 986447360,
    987017472, 987587904, 988157952, 988729088, 989300416, 989872192,
    990444224, 991016000, 991588672, 992161728, 992735168, 993308864,
    993882880, 994456576, 995031296, 995606336, 996181696, 996757440,
    997332800, 997909184, 998485888, 999062912, 999640256, 1000217984,
    1000795392, 1001373696, 1001952448, 1002531520, 1003110848, 1003689920,
    1004270016, 1004850304, 1005431040, 1006012160, 1006592832, 1007174592,
    1007756608, 1008339008, 1008921792, 1009504768, 1010087552, 1010671296,
    1011255360, 1011839808, 1012424576, 1013009024, 1013594368, 1014180160,
    1014766272, 1015352768, 1015938880, 1016526016, 1017113472, 1017701248,
    1018289408, 1018877824, 1019465984, 1020055104, 1020644672, 1021234496,
    1021824768, 1022414528, 1023005440, 1023596608, 1024188160, 1024780096,
    1025371584, 1025964160, 1026557120, 1027150336, 1027744000, 1028337920,
    1028931520, 1029526144, 1030121152, 1030716480, 1031312128, 1031907456,
    1032503808, 1033100480, 1033697536, 1034294912, 1034892032, 1035490048,
    1036088512, 1036687232, 1037286336, 1037885824, 1038484928, 1039085056,
    1039685632, 1040286464, 1040887680, 1041488448, 1042090368, 1042692608,
    1043295168, 1043898176, 1044501440, 1045104384, 1045708288, 1046312640,
    1046917376, 1047522368, 1048127040, 1048732800, 1049338816, 1049945280,
    1050552128, 1051158528, 1051765952, 1052373824, 1052982016, 1053590592,
    1054199424, 1054807936, 1055417600, 1056027456, 1056637760, 1057248448,
    1057858752, 1058470016, 1059081728, 1059693824, 1060306304, 1060918336,
    1061531392, 1062144896, 1062758656, 1063372928, 1063987392, 1064601664,
    1065216896, 1065832448, 1066448448, 1067064704, 1067680704, 1068297728,
    1068915136, 1069532864, 1070150976, 1070768640, 1071387520, 1072006720,
    1072626240, 1073246080, 1073866368, 1074486272, 1075107200, 1075728512,
    1076350208, 1076972160, 1077593856, 1078216704, 1078839680, 1079463296,
    1080087040, 1080710528, 1081335168, 1081960064, 1082585344, 1083211008,
    1083836928, 1084462592, 1085089280, 1085716352, 1086343936, 1086971648,
    1087599104, 1088227712, 1088856576, 1089485824, 1090115456, 1090745472,
    1091375104, 1092005760, 1092636928, 1093268352, 1093900160, 1094531584,
    1095164160, 1095796992, 1096430336, 1097064064, 1097697280, 1098331648,
    1098966400, 1099601536, 1100237056, 1100872832, 1101508224, 1102144768,
    1102781824, 1103419136, 1104056832, 1104694144, 1105332608, 1105971328,
    1106610432, 1107249920, 1107889152, 1108529408, 1109170048, 1109811072,
    1110452352, 1111094144, 1111735552, 1112377984, 1113020928, 1113664128,
    1114307712, 1114950912, 1115595264, 1116240000, 1116885120, 1117530624,
    1118175744, 1118821888, 1119468416, 1120115456, 1120762752, 1121410432,
    1122057856, 1122706176, 1123355136, 1124004224, 1124653824, 1125303040,
    1125953408, 1126604160, 1127255168, 1127906560, 1128557696, 1129209984,
    1129862528, 1130515456, 1131168768, 1131822592, 1132475904, 1133130368,
    1133785216, 1134440448, 1135096064, 1135751296, 1136407680, 1137064448,
    1137721472, 1138379008, 1139036800, 1139694336, 1140353024, 1141012096,
    1141671424, 1142331264, 1142990592, 1143651200, 1144312192, 1144973440,
    1145635200, 1146296448, 1146958976, 1147621760, 1148285056, 1148948608,
    1149612672, 1150276224, 1150940928, 1151606144, 1152271616, 1152937600,
    1153603072, 1154269824, 1154936832, 1155604352, 1156272128, 1156939648,
    1157608192, 1158277248, 1158946560, 1159616384, 1160286464, 1160956288,
    1161627264, 1162298624, 1162970240, 1163642368, 1164314112, 1164987008,
    1165660160, 1166333824, 1167007872, 1167681536, 1168356352, 1169031552,
    1169707136, 1170383104, 1171059584, 1171735552, 1172412672, 1173090304,
    1173768192, 1174446592, 1175124480, 1175803648, 1176483072, 1177163008,
    1177843328, 1178523264, 1179204352, 1179885824, 1180567680, 1181249920,
    1181932544, 1182614912, 1183298304, 1183982208, 1184666368, 1185351040,
    1186035328, 1186720640, 1187406464, 1188092672, 1188779264, 1189466368,
    1190152960, 1190840832, 1191528960, 1192217600, 1192906624, 1193595136,
    1194285056, 1194975232, 1195665792, 1196356736, 1197047296, 1197739136,
    1198431360, 1199123968, 1199816960, 1200510336, 1201203328, 1201897600,
    1202592128, 1203287040, 1203982464, 1204677504, 1205373696, 1206070272,
    1206767232, 1207464704, 1208161664, 1208859904, 1209558528, 1210257536,
    1210956928, 1211656832, 1212356224, 1213056768, 1213757952, 1214459392,
    1215161216, 1215862656, 1216565376, 1217268352, 1217971840, 1218675712,
    1219379200, 1220083840, 1220788992, 1221494528, 1222200448, 1222906752,
    1223612672, 1224319872, 1225027456, 1225735424, 1226443648, 1227151616,
    1227860864, 1228570496, 1229280512, 1229990912, 1230700928, 1231412096,
    1232123776, 1232835840, 1233548288, 1234261248, 1234973696, 1235687424,
    1236401536, 1237116032, 1237831040, 1238545536, 1239261312, 1239977472,
    1240694144, 1241411072, 1242128512, 1242845568, 1243563776, 1244282496,
    1245001600, 1245721088, 1246440192, 1247160448, 1247881216, 1248602368,
    1249324032, 1250045184, 1250767616, 1251490432, 1252213632, 1252937344,
    1253661440, 1254385152, 1255110016, 1255835392, 1256561152, 1257287424,
    1258013184, 1258740096, 1259467648, 1260195456, 1260923648, 1261651584,
    1262380800, 1263110272, 1263840256, 1264570624, 1265301504, 1266031872,
    1266763520, 1267495552, 1268227968, 1268961024, 1269693440, 1270427264,
    1271161472, 1271896064, 1272631168, 1273365760, 1274101632, 1274838016,
    1275574784, 1276311808, 1277049472, 1277786624, 1278525056, 1279264000,
    1280003328, 1280743040, 1281482368, 1282222976, 1282963968, 1283705344,
    1284447232, 1285188736, 1285931392, 1286674560, 1287418240, 1288162176,
    1288906624, 1289650688, 1290395904, 1291141760, 1291887872, 1292634496,
    1293380608, 1294128128, 1294875904, 1295624320, 1296373120, 1297122304,
    1297870976, 1298621056, 1299371520, 1300122496, 1300873856, 1301624832,
    1302376960, 1303129600, 1303882752, 1304636288, 1305389312, 1306143872,
    1306898688, 1307654016, 1308409600, 1309165696, 1309921536, 1310678528,
    1311435904, 1312193920, 1312952192, 1313710080, 1314469248, 1315228928,
    1315988992, 1316749568, 1317509632, 1318271104, 1319032960, 1319795200,
    1320557952, 1321321088, 1322083840, 1322847872, 1323612416, 1324377216,
    1325142656, 1325907584, 1326673920, 1327440512, 1328207744, 1328975360,
    1329742464, 1330510976, 1331279872, 1332049152, 1332819072, 1333589248,
    1334359168, 1335130240, 1335901824, 1336673920, 1337446400, 1338218368,
    1338991744, 1339765632, 1340539904, 1341314560, 1342088832, 1342864512,
    1343640576, 1344417024, 1345193984, 1345971456, 1346748416, 1347526656,
    1348305408, 1349084672, 1349864320, 1350643456, 1351424000, 1352205056,
    1352986496, 1353768448, 1354550784, 1355332608, 1356115968, 1356899712,
    1357683840, 1358468480, 1359252608, 1360038144, 1360824192, 1361610624,
    1362397440, 1363183872, 1363971712, 1364760064, 1365548672, 1366337792,
    1367127424, 1367916672, 1368707200, 1369498240, 1370289664, 1371081472,
    1371873024, 1372665856, 1373459072, 1374252800, 1375047040, 1375840768,
    1376635904, 1377431552, 1378227584, 1379024000, 1379820928, 1380617472,
    1381415296, 1382213760, 1383012480, 1383811840, 1384610560, 1385410816,
    1386211456, 1387012480, 1387814144, 1388615168, 1389417728, 1390220672,
    1391024128, 1391827968, 1392632320, 1393436288, 1394241536, 1395047296,
    1395853568, 1396660224, 1397466368, 1398274048, 1399082112, 1399890688,
    1400699648, 1401508224, 1402318080, 1403128576, 1403939456, 1404750848,
    1405562624, 1406374016, 1407186816, 1408000000, 1408813696, 1409627904,
    1410441728, 1411256704, 1412072320, 1412888320, 1413704960, 1414521856,
    1415338368, 1416156288, 1416974720, 1417793664, 1418612992, 1419431808,
    1420252160, 1421072896, 1421894144, 1422715904, 1423537280, 1424359808,
    1425183104, 1426006784, 1426830848, 1427655296, 1428479488, 1429305088,
    1430131072, 1430957568, 1431784576, 1432611072, 1433438976, 1434267392,
    1435096192, 1435925632, 1436754432, 1437584768, 1438415616, 1439246848,
    1440078720, 1440910848, 1441742720, 1442575872, 1443409664, 1444243584,
    1445078400, 1445912576, 1446748032, 1447584256, 1448420864, 1449257856,
    1450094464, 1450932480, 1451771008, 1452609920, 1453449472, 1454289408,
    1455128960, 1455969920, 1456811264, 1457653248, 1458495616, 1459337600,
    1460180864, 1461024768, 1461869056, 1462713984, 1463558272, 1464404096,
    1465250304, 1466097152, 1466944384, 1467792128, 1468639488, 1469488256,
    1470337408, 1471187200, 1472037376, 1472887168, 1473738368, 1474589952,
    1475442304, 1476294912, 1477148160, 1478000768, 1478854912, 1479709696,
    1480564608, 1481420288, 1482275456, 1483132160, 1483989248, 1484846976,
    1485704960, 1486562688, 1487421696, 1488281344, 1489141504, 1490002048,
    1490863104, 1491723776, 1492585856, 1493448448, 1494311424, 1495175040,
    1496038144, 1496902656, 1497767808, 1498633344, 1499499392, 1500365056,
    1501232128, 1502099712, 1502967808, 1503836416, 1504705536, 1505574016,
    1506444032, 1507314688, 1508185856, 1509057408, 1509928576, 1510801280,
    1511674240, 1512547840, 1513421952, 1514295680, 1515170816, 1516046464,
    1516922624, 1517799296, 1518676224, 1519552896, 1520431104, 1521309824,
    1522188928, 1523068800, 1523948032, 1524828672, 1525709824, 1526591616,
    1527473792, 1528355456, 1529238784, 1530122496, 1531006720, 1531891712,
    1532776832, 1533661824, 1534547968, 1535434880, 1536322304, 1537210112,
    1538097408, 1538986368, 1539875840, 1540765696, 1541656192, 1542547072,
    1543437440, 1544329472, 1545221888, 1546114944, 1547008384, 1547901440,
    1548796032, 1549691136, 1550586624, 1551482752, 1552378368, 1553275520,
    1554173184, 1555071232, 1555970048, 1556869248, 1557767936, 1558668288,
    1559568896, 1560470272, 1561372032, 1562273408, 1563176320, 1564079616,
    1564983424, 1565888000, 1566791808, 1567697408, 1568603392, 1569509760,
    1570416896, 1571324416, 1572231424, 1573140096, 1574049152, 1574958976,
    1575869184, 1576778752, 1577689984, 1578601728, 1579514112, 1580426880,
    1581339264, 1582253056, 1583167488, 1584082432, 1584997888, 1585913984,
    1586829440, 1587746304, 1588663936, 1589582080, 1590500736, 1591418880,
    1592338560, 1593258752, 1594179584, 1595100928, 1596021632, 1596944000,
    1597866880, 1598790272, 1599714304, 1600638848, 1601562752, 1602488320,
    1603414272, 1604340992, 1605268224, 1606194816, 1607123072, 1608051968,
    1608981120, 1609911040, 1610841344, 1611771264, 1612702848, 1613634688,
    1614567168, 1615500288, 1616432896, 1617367040, 1618301824, 1619237120,
    1620172800, 1621108096, 1622044928, 1622982272, 1623920128, 1624858752,
    1625797632, 1626736256, 1627676416, 1628616960, 1629558272, 1630499968,
    1631441152, 1632384000, 1633327232, 1634271232, 1635215744, 1636159744,
    1637105152, 1638051328, 1638998016, 1639945088, 1640892928, 1641840128,
    1642788992, 1643738368, 1644688384, 1645638784, 1646588672, 1647540352,
    1648492416, 1649445120, 1650398464, 1651351168, 1652305408, 1653260288,
    1654215808, 1655171712, 1656128256, 1657084288, 1658041856, 1659000064,
    1659958784, 1660918272, 1661876992, 1662837376, 1663798400, 1664759936,
    1665721984, 1666683520, 1667646720, 1668610560, 1669574784, 1670539776,
    1671505024, 1672470016, 1673436544 };


#if 0 /* NOT NEEDED USES TOO MUCH CPU */

void _WM_DynamicVolumeAdjust(struct _mdi *mdi, int32_t *tmp_buffer, uint32_t buffer_used) {

    uint32_t i = 0;
    uint32_t j = 0;

    int8_t peak_set = 0;
    int32_t prev_val = 0;
    uint32_t peak_ofs = 0;
    int32_t peak = mdi->dyn_vol_peak;

    double volume_to_reach = mdi->dyn_vol_to_reach;
    double volume = mdi->dyn_vol;
    double volume_adjust = mdi->dyn_vol_adjust;
    double tmp_output = 0.0;

#define MAX_DYN_VOL 1.0

    for (i = 0; i < buffer_used; i++) {
        if ((i == 0) || (i > peak_ofs)) {
            // Find Next Peak/Troff
            peak_set = 0;
            prev_val = peak;
            peak_ofs = 0;
            for (j = i; j < buffer_used; j++) {
                if (peak_set == 0) {
                    // find what direction the data is going
                    if (prev_val > tmp_buffer[j]) {
                        // Going Down
                        peak_set = -1;
                    } else if (prev_val < tmp_buffer[j]) {
                        // Doing Up
                        peak_set = 1;
                    } else {
                        // No direction, keep looking
                        prev_val = tmp_buffer[j];
                        continue;
                    }
                }

                if (peak_set == 1) {
                    // Data is going up
                    if (peak < tmp_buffer[j]) {
                        peak = tmp_buffer[j];
                        peak_ofs = j;
                    } else if (peak > tmp_buffer[j]) {
                        // Data is starting to go down, we found the peak
                        break;
                    }
                } else { // assume peak_set == -1
                    // Data is going down
                    if (peak > tmp_buffer[j]) {
                        peak = tmp_buffer[j];
                        peak_ofs = j;
                    } else if (peak < tmp_buffer[j]) {
                        // Data is starting to go up, we found the troff
                        break;
                    }
                }

                prev_val = tmp_buffer[j];
            }

            if (peak_set != 0) {
                if (peak_set == 1) {
                    if (peak > 32767) {
                        volume_to_reach = 32767.0 / (double)peak;
                    } else {
                        volume_to_reach = MAX_DYN_VOL;
                    }
                } else { // assume peak_set == -1
                    if (peak < -32768) {
                        volume_to_reach = -32768.0 / (double)peak;
                    } else {
                        volume_to_reach = MAX_DYN_VOL;
                    }
                }
            } else {
                // No peak found, set volume we want to normal
                volume_to_reach = MAX_DYN_VOL;
            }

            if (volume != volume_to_reach) {
                if (volume_to_reach == MAX_DYN_VOL) {
                    // if we want normal volume then adjust to it slower
                    volume_adjust = (volume_to_reach - volume) / ((double)_WM_SampleRate * 0.1);
                } else {
                    // if we want to clamp the volume then adjust quickly
                    volume_adjust = (volume_to_reach - volume) / ((double)_WM_SampleRate * 0.0001);
                }
            }
        }

        // First do we need to do volume adjustments
        if ((volume_adjust != 0.0) && (volume != volume_to_reach)) {
                volume += volume_adjust;
                if (volume_adjust > 0.0) {
                    // if increasing the volume
                    if (volume >= MAX_DYN_VOL) {
                        // we dont boost volume
                        volume = MAX_DYN_VOL;
                        volume_adjust = 0.0;
                    } else if (volume > volume_to_reach) {
                        // we dont want to go above the level we wanted
                        volume = volume_to_reach;
                        volume_adjust = 0.0;
                    }
                } else {
                    // decreasing the volume
                    if (volume < volume_to_reach) {
                        // we dont want to go below the level we wanted
                        volume = volume_to_reach;
                        volume_adjust = 0.0;
                    }
                }
            }

        // adjust buffer volume
        tmp_output = (double)tmp_buffer[i] * volume;
        tmp_buffer[i] = (int32_t)tmp_output;
    }

    // store required values
    mdi->dyn_vol_adjust = volume_adjust;
    mdi->dyn_vol_peak = peak;
    mdi->dyn_vol = volume;
    mdi->dyn_vol_to_reach = volume_to_reach;
}

#endif

/* Should be called in any function that effects note volumes */
void _WM_AdjustNoteVolumes(struct _mdi *mdi, uint8_t ch, struct _note *nte) {
    float premix_dBm;
    float premix_lin;
    uint8_t pan_ofs;
    float premix_dBm_left;
    float premix_dBm_right;
    float premix_left;
    float premix_right;
    float volume_adj;
    uint32_t vol_ofs;

    /*
     Pointless CPU heating checks to shoosh up a compiler
     */
    if (ch > 0x0f) ch = 0x0f;

    if (nte->ignore_chan_events) return;

    pan_ofs = mdi->channel[ch].balance + mdi->channel[ch].pan - 64;

    vol_ofs = (nte->velocity * ((mdi->channel[ch].expression * mdi->channel[ch].volume) / 127)) / 127;

    /*
     This value is to reduce the chance of clipping.
     Higher value means lower overall volume,
     Lower value means higher overall volume.
     NOTE: The lower the value the higher the chance of clipping.
     FIXME: Still needs tuning. Clipping heard at a value of 3.75
     */
#define VOL_DIVISOR 4.0f
    volume_adj = ((float)_WM_MasterVolume / 1024.0f) / VOL_DIVISOR;

    MIDI_EVENT_DEBUG(__FUNCTION__,ch, 0);

    if (pan_ofs > 127) pan_ofs = 127;
    premix_dBm_left = dBm_pan_volume[(127-pan_ofs)];
    premix_dBm_right = dBm_pan_volume[pan_ofs];

    if (mdi->extra_info.mixer_options & WM_MO_LOG_VOLUME) {
        premix_dBm = dBm_volume[vol_ofs];

        premix_dBm_left += premix_dBm;
        premix_dBm_right += premix_dBm;

        premix_left = (powf(10.0f, (premix_dBm_left / 20.0f))) * volume_adj;
        premix_right = (powf(10.0f, (premix_dBm_right / 20.0f))) * volume_adj;
    } else {
        premix_lin = (float)(_WM_lin_volume[vol_ofs]) / 1024.0f;

        premix_left = premix_lin * powf(10.0f, (premix_dBm_left / 20)) * volume_adj;
        premix_right = premix_lin * powf(10.0f, (premix_dBm_right / 20)) * volume_adj;
    }
    nte->left_mix_volume = (int32_t)(premix_left * 1024.0f);
    nte->right_mix_volume = (int32_t)(premix_right * 1024.0f);
}

/* Should be called in any function that effects channel volumes */
/* Calling this function with a value > 15 will make it adjust notes on all channels */
void _WM_AdjustChannelVolumes(struct _mdi *mdi, uint8_t ch) {
    struct _note *nte = mdi->note;
    if (nte != NULL) {
        do {
            if (ch <= 15) {
                if ((nte->noteid >> 8) == ch) {
                    goto _DO_ADJUST;
                }
            } else {
            _DO_ADJUST:
                if (!nte->ignore_chan_events) {
                    _WM_AdjustNoteVolumes(mdi, ch, nte);
                    if (nte->replay) _WM_AdjustNoteVolumes(mdi, ch, nte->replay);
                }
            }
            nte = nte->next;
        } while (nte != NULL);
    }
}

float _WM_GetSamplesPerTick(uint32_t divisions, uint32_t tempo) {
    float microseconds_per_tick;
    float secs_per_tick;
    float samples_per_tick;

    /* Slow but needed for accuracy */
    microseconds_per_tick = (float) tempo / (float) divisions;
    secs_per_tick = microseconds_per_tick / 1000000.0f;
    samples_per_tick = _WM_SampleRate * secs_per_tick;

    return (samples_per_tick);
}

static void _WM_CheckEventMemoryPool(struct _mdi *mdi) {
    if ((mdi->event_count + 1) >= mdi->events_size) {
        mdi->events_size += MEM_CHUNK;
        mdi->events = (struct _event *) realloc(mdi->events,
                              (mdi->events_size * sizeof(struct _event)));
    }
}

void _WM_do_note_off_extra(struct _note *nte) {

    MIDI_EVENT_DEBUG(__FUNCTION__,0, 0);
    nte->is_off = 0;
        {
        if (!(nte->modes & SAMPLE_ENVELOPE)) {
            if (nte->modes & SAMPLE_LOOP) {
                nte->modes ^= SAMPLE_LOOP;
            }
            nte->env_inc = 0;

        } else if (nte->hold) {
            nte->hold |= HOLD_OFF;
/*
        } else if (nte->modes & SAMPLE_SUSTAIN) {
            if (nte->env < 3) {
                nte->env = 3;
                if (nte->env_level > nte->sample->env_target[3]) {
                    nte->env_inc = -nte->sample->env_rate[3];
                } else {
                    nte->env_inc = nte->sample->env_rate[3];
                }
            }
*/
        } else if (nte->modes & SAMPLE_CLAMPED) {
            if (nte->env < 5) {
                nte->env = 5;
                if (nte->env_level > nte->sample->env_target[5]) {
                    nte->env_inc = -nte->sample->env_rate[5];
                } else {
                    nte->env_inc = nte->sample->env_rate[5];
                }
            }
        } else if (nte->env < 3) {
            nte->env = 3;
            if (nte->env_level > nte->sample->env_target[3]) {
                nte->env_inc = -nte->sample->env_rate[3];
            } else {
                nte->env_inc = nte->sample->env_rate[3];
            }
        }
    }
}


void _WM_do_midi_divisions(struct _mdi *mdi, struct _event_data *data) {
    // placeholder function so we can record divisions in the event stream
    // for conversion function _WM_Event2Midi()
    UNUSED(mdi);
    UNUSED(data);
    return;
}

void _WM_do_note_off(struct _mdi *mdi, struct _event_data *data) {
    struct _note *nte;
    uint8_t ch = data->channel;

    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    nte = &mdi->note_table[0][ch][(data->data.value >> 8)];
    if (!nte->active) {
        nte = &mdi->note_table[1][ch][(data->data.value >> 8)];
        if (!nte->active) {
            return;
        }
    }

    if ((mdi->channel[ch].isdrum) && (!(nte->modes & SAMPLE_LOOP))) {
        return;
    }

    if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env == 0)) {
        // This is a fix for notes that end before the
        // initial step of the envelope has completed
        // making it impossible to hear them at times.
        nte->is_off = 1;
    } else {
        _WM_do_note_off_extra(nte);
    }
}

static inline uint32_t get_inc(struct _mdi *mdi, struct _note *nte) {
    int ch = nte->noteid >> 8;
    int32_t note_f;
    uint32_t freq;

    if (__builtin_expect((nte->patch->note != 0), 0)) {
        note_f = nte->patch->note * 100;
    } else {
        note_f = (nte->noteid & 0x7f) * 100;
    }
    note_f += mdi->channel[ch].pitch_adjust;
    if (__builtin_expect((note_f < 0), 0)) {
        note_f = 0;
    } else if (__builtin_expect((note_f > 12700), 0)) {
        note_f = 12700;
    }
    freq = _WM_freq_table[(note_f % 1200)] >> (10 - (note_f / 1200));
    return (((freq / ((_WM_SampleRate * 100) / 1024)) * 1024
             / nte->sample->inc_div));
}

void _WM_do_note_on(struct _mdi *mdi, struct _event_data *data) {
    struct _note *nte;
    struct _note *prev_nte;
    struct _note *nte_array;
    uint32_t freq = 0;
    struct _patch *patch;
    struct _sample *sample;
    uint8_t ch = data->channel;
    uint8_t note = (data->data.value >> 8);
    uint8_t velocity = (data->data.value & 0xFF);

    if (velocity == 0x00) {
        _WM_do_note_off(mdi, data);
        return;
    }

    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if (!mdi->channel[ch].isdrum) {
        patch = mdi->channel[ch].patch;
        if (patch == NULL) {
            return;
        }
        freq = _WM_freq_table[(note % 12) * 100] >> (10 - (note / 12));
    } else {
        patch = _WM_get_patch_data(mdi,
                               ((mdi->channel[ch].bank << 8) | note | 0x80));
        if (patch == NULL) {
            return;
        }
        if (patch->note) {
            freq = _WM_freq_table[(patch->note % 12) * 100]
            >> (10 - (patch->note / 12));
        } else {
            freq = _WM_freq_table[(note % 12) * 100] >> (10 - (note / 12));
        }
    }

    sample = _WM_get_sample_data(patch, (freq / 100));
    if (sample == NULL) {
        return;
    }

    nte = &mdi->note_table[0][ch][note];

    if (nte->active) {
        if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3)
            && (!(nte->hold & HOLD_OFF)))
            return;
        nte->replay = &mdi->note_table[1][ch][note];
        nte->env = 6;
        nte->env_inc = -nte->sample->env_rate[6];
        nte = nte->replay;
    } else {
        if (mdi->note_table[1][ch][note].active) {
            if ((nte->modes & SAMPLE_ENVELOPE) && (nte->env < 3)
                && (!(nte->hold & HOLD_OFF)))
                return;
            mdi->note_table[1][ch][note].replay = nte;
            mdi->note_table[1][ch][note].env = 6;
            mdi->note_table[1][ch][note].env_inc =
            -mdi->note_table[1][ch][note].sample->env_rate[6];
        } else {
            nte_array = mdi->note;
            if (nte_array == NULL) {
                mdi->note = nte;
            } else {
                do {
                    prev_nte = nte_array;
                    nte_array = nte_array->next;
                } while (nte_array);
                prev_nte->next = nte;
            }
            nte->active = 1;
            nte->next = NULL;
        }
    }
    nte->noteid = (ch << 8) | note;
    nte->patch = patch;
    nte->sample = sample;
    nte->sample_pos = 0;
    nte->sample_inc = get_inc(mdi, nte);
    nte->velocity = velocity;
    nte->env = 0;
    nte->env_inc = nte->sample->env_rate[0];
    nte->env_level = 0;
    nte->modes = sample->modes;
    nte->hold = mdi->channel[ch].hold;
    nte->replay = NULL;
    nte->is_off = 0;
    nte->ignore_chan_events = 0;
    _WM_AdjustNoteVolumes(mdi, ch, nte);
}

void _WM_do_aftertouch(struct _mdi *mdi, struct _event_data *data) {
    struct _note *nte;
    uint8_t ch = data->channel;

    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    nte = &mdi->note_table[0][ch][(data->data.value >> 8)];
    if (!nte->active) {
        nte = &mdi->note_table[1][ch][(data->data.value >> 8)];
        if (!nte->active) {
            return;
        }
    }

    nte->velocity = data->data.value & 0xff;
    _WM_AdjustNoteVolumes(mdi, ch, nte);
    if (nte->replay) {
        nte->replay->velocity = data->data.value & 0xff;
        _WM_AdjustNoteVolumes(mdi, ch, nte->replay);
    }
}

void _WM_do_control_bank_select(struct _mdi *mdi, struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);
    mdi->channel[ch].bank = data->data.value;
}

void _WM_do_control_data_entry_course(struct _mdi *mdi,
                                         struct _event_data *data) {
    uint8_t ch = data->channel;
    int data_tmp;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if ((mdi->channel[ch].reg_non == 0)
        && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
        data_tmp = mdi->channel[ch].pitch_range % 100;
        mdi->channel[ch].pitch_range = data->data.value * 100 + data_tmp;
    /*  printf("Data Entry Course: pitch_range: %i\n\r",mdi->channel[ch].pitch_range);*/
    /*  printf("Data Entry Course: data %li\n\r",data->data.value);*/
    }
}

void _WM_do_control_channel_volume(struct _mdi *mdi,
                                      struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    mdi->channel[ch].volume = data->data.value;
    _WM_AdjustChannelVolumes(mdi, ch);
}

void _WM_do_control_channel_balance(struct _mdi *mdi,
                                       struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    mdi->channel[ch].balance = data->data.value;
    _WM_AdjustChannelVolumes(mdi, ch);
}

void _WM_do_control_channel_pan(struct _mdi *mdi, struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    mdi->channel[ch].pan = data->data.value;
    _WM_AdjustChannelVolumes(mdi, ch);
}

void _WM_do_control_channel_expression(struct _mdi *mdi,
                                          struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    mdi->channel[ch].expression = data->data.value;
    _WM_AdjustChannelVolumes(mdi, ch);
}

void _WM_do_control_data_entry_fine(struct _mdi *mdi,
                                       struct _event_data *data) {
    uint8_t ch = data->channel;
    int data_tmp;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if ((mdi->channel[ch].reg_non == 0)
      && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
        data_tmp = mdi->channel[ch].pitch_range / 100;
        mdi->channel[ch].pitch_range = (data_tmp * 100) + data->data.value;
    /*  printf("Data Entry Fine: pitch_range: %i\n\r",mdi->channel[ch].pitch_range);*/
    /*  printf("Data Entry Fine: data: %li\n\r", data->data.value);*/
    }
}

void _WM_do_control_channel_hold(struct _mdi *mdi, struct _event_data *data) {
    struct _note *note_data = mdi->note;
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if (data->data.value > 63) {
        mdi->channel[ch].hold = 1;
    } else {
        mdi->channel[ch].hold = 0;
        if (note_data) {
            do {
                if ((note_data->noteid >> 8) == ch) {
                    if (note_data->hold & HOLD_OFF) {
                        if (note_data->modes & SAMPLE_ENVELOPE) {
                            if (note_data->modes & SAMPLE_CLAMPED) {
                                if (note_data->env < 5) {
                                    note_data->env = 5;
                                    if (note_data->env_level
                                        > note_data->sample->env_target[5]) {
                                        note_data->env_inc =
                                        -note_data->sample->env_rate[5];
                                    } else {
                                        note_data->env_inc =
                                        note_data->sample->env_rate[5];
                                    }
                                }
                            /*
                            } else if (note_data->modes & SAMPLE_SUSTAIN) {
                                if (note_data->env < 3) {
                                    note_data->env = 3;
                                    if (note_data->env_level
                                        > note_data->sample->env_target[3]) {
                                        note_data->env_inc =
                                        -note_data->sample->env_rate[3];
                                    } else {
                                        note_data->env_inc =
                                        note_data->sample->env_rate[3];
                                    }
                                }
                             */
                             } else if (note_data->env < 3) {
                                note_data->env = 3;
                                if (note_data->env_level
                                    > note_data->sample->env_target[3]) {
                                    note_data->env_inc =
                                    -note_data->sample->env_rate[3];
                                } else {
                                    note_data->env_inc =
                                    note_data->sample->env_rate[3];
                                }
                            }
                        } else {
                            if (note_data->modes & SAMPLE_LOOP) {
                                note_data->modes ^= SAMPLE_LOOP;
                            }
                            note_data->env_inc = 0;
                        }
                    }
                    note_data->hold = 0x00;
                }
                note_data = note_data->next;
            } while (note_data);
        }
    }
}

void _WM_do_control_data_increment(struct _mdi *mdi,
                                      struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if ((mdi->channel[ch].reg_non == 0)
        && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
        if (mdi->channel[ch].pitch_range < 0x3FFF)
            mdi->channel[ch].pitch_range++;
    }
}

void _WM_do_control_data_decrement(struct _mdi *mdi,
                                      struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if ((mdi->channel[ch].reg_non == 0)
        && (mdi->channel[ch].reg_data == 0x0000)) { /* Pitch Bend Range */
        if (mdi->channel[ch].pitch_range > 0)
            mdi->channel[ch].pitch_range--;
    }
}
void _WM_do_control_non_registered_param_fine(struct _mdi *mdi,
                                            struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);
    mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x3F80)
                                | data->data.value;
    mdi->channel[ch].reg_non = 1;
}

void _WM_do_control_non_registered_param_course(struct _mdi *mdi,
                                     struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);
    mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x7F)
                                | (data->data.value << 7);
    mdi->channel[ch].reg_non = 1;
}

void _WM_do_control_registered_param_fine(struct _mdi *mdi,
                                          struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);
    mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x3F80)
                                | data->data.value;
    mdi->channel[ch].reg_non = 0;
}

void _WM_do_control_registered_param_course(struct _mdi *mdi,
                                            struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);
    mdi->channel[ch].reg_data = (mdi->channel[ch].reg_data & 0x7F)
                                | (data->data.value << 7);
    mdi->channel[ch].reg_non = 0;
}

void _WM_do_control_channel_sound_off(struct _mdi *mdi,
                                      struct _event_data *data) {
    struct _note *note_data = mdi->note;
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if (note_data) {
        do {
            if ((note_data->noteid >> 8) == ch) {
                note_data->active = 0;
                if (note_data->replay) {
                    note_data->replay = NULL;
                }
            }
            note_data = note_data->next;
        } while (note_data);
    }
}

void _WM_do_control_channel_controllers_off(struct _mdi *mdi,
                                            struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    mdi->channel[ch].expression = 127;
    mdi->channel[ch].pressure = 127;
    mdi->channel[ch].reg_data = 0xffff;
    mdi->channel[ch].pitch_range = 200;
    mdi->channel[ch].pitch = 0;
    mdi->channel[ch].pitch_adjust = 0;
    mdi->channel[ch].hold = 0;

    _WM_AdjustChannelVolumes(mdi, ch);
}

void _WM_do_control_channel_notes_off(struct _mdi *mdi,
                                      struct _event_data *data) {
    struct _note *note_data = mdi->note;
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if (mdi->channel[ch].isdrum)
        return;
    if (note_data) {
        do {
            if ((note_data->noteid >> 8) == ch) {
                if (!note_data->hold) {
                    if (note_data->modes & SAMPLE_ENVELOPE) {
                        if (note_data->env < 5) {
                            if (note_data->env_level
                                > note_data->sample->env_target[5]) {
                                note_data->env_inc =
                                -note_data->sample->env_rate[5];
                            } else {
                                note_data->env_inc =
                                note_data->sample->env_rate[5];
                            }
                            note_data->env = 5;
                        }
                    }
                } else {
                    note_data->hold |= HOLD_OFF;
                }
            }
            note_data = note_data->next;
        } while (note_data);
    }
}

void _WM_do_control_dummy(struct _mdi *mdi, struct _event_data *data) {
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
}

void _WM_do_patch(struct _mdi *mdi, struct _event_data *data) {
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);
    if (!mdi->channel[ch].isdrum) {
        mdi->channel[ch].patch = _WM_get_patch_data(mdi,
                                                ((mdi->channel[ch].bank << 8) | data->data.value));
    } else {
        mdi->channel[ch].bank = data->data.value;
    }
}

void _WM_do_channel_pressure(struct _mdi *mdi, struct _event_data *data) {
    uint8_t ch = data->channel;
    struct _note *note_data = mdi->note;
    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    mdi->channel[ch].pressure = data->data.value;

    while (note_data) {
        if (!note_data->ignore_chan_events) {
            if ((note_data->noteid >> 8) == ch) {
                note_data->velocity = data->data.value & 0xff;
                _WM_AdjustNoteVolumes(mdi, ch, note_data);
                if (note_data->replay) {
                    note_data->replay->velocity = data->data.value & 0xff;
                    _WM_AdjustNoteVolumes(mdi, ch, note_data->replay);
                }
            }
        }
        note_data = note_data->next;
    }
}

void _WM_do_pitch(struct _mdi *mdi, struct _event_data *data) {
    struct _note *note_data = mdi->note;
    uint8_t ch = data->channel;

    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);
    mdi->channel[ch].pitch = data->data.value - 0x2000;

    if (mdi->channel[ch].pitch < 0) {
        mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range
        * mdi->channel[ch].pitch / 8192;
    } else {
        mdi->channel[ch].pitch_adjust = mdi->channel[ch].pitch_range
        * mdi->channel[ch].pitch / 8191;
    }

    if (note_data) {
        do {
            if ((note_data->noteid >> 8) == ch) {
                note_data->sample_inc = get_inc(mdi, note_data);
            }
            note_data = note_data->next;
        } while (note_data);
    }
}

void _WM_do_sysex_roland_drum_track(struct _mdi *mdi, struct _event_data *data) {
    uint8_t ch = data->channel;

    MIDI_EVENT_DEBUG(__FUNCTION__,ch, data->data.value);

    if (data->data.value > 0) {
        mdi->channel[ch].isdrum = 1;
        mdi->channel[ch].patch = NULL;
    } else {
        mdi->channel[ch].isdrum = 0;
        mdi->channel[ch].patch = _WM_get_patch_data(mdi, 0);
    }
}

void _WM_do_sysex_gm_reset(struct _mdi *mdi, struct _event_data *data) {
    int i;

    if (data != NULL) {
        MIDI_EVENT_DEBUG(__FUNCTION__,data->channel, data->data.value);
    } else {
        MIDI_EVENT_DEBUG(__FUNCTION__,0, 0);
    }

    for (i = 0; i < 16; i++) {
        mdi->channel[i].bank = 0;
        if (i != 9) {
            mdi->channel[i].patch = _WM_get_patch_data(mdi, 0);
        } else {
            mdi->channel[i].patch = NULL;
        }
        mdi->channel[i].hold = 0;
        mdi->channel[i].volume = 100;
        mdi->channel[i].pressure = 127;
        mdi->channel[i].expression = 127;
        mdi->channel[i].balance = 64;
        mdi->channel[i].pan = 64;
        mdi->channel[i].pitch = 0;
        mdi->channel[i].pitch_range = 200;
        mdi->channel[i].reg_data = 0xFFFF;
        mdi->channel[i].isdrum = 0;
    }
    /* I would not expect notes to be active when this event
     triggers but we'll adjust active notes as well just in case */
    _WM_AdjustChannelVolumes(mdi,16); // A setting > 15 adjusts all channels

    mdi->channel[9].isdrum = 1;
}

void _WM_do_sysex_roland_reset(struct _mdi *mdi, struct _event_data *data) {
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    _WM_do_sysex_gm_reset(mdi,data);
}

void _WM_do_sysex_yamaha_reset(struct _mdi *mdi, struct _event_data *data) {
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    _WM_do_sysex_gm_reset(mdi,data);
}

void _WM_Release_Allowance(struct _mdi *mdi) {
    uint32_t release = 0;
    uint32_t longest_release = 0;
    
    struct _note *note = mdi->note;
    
    while (note != NULL) {
        
        if (note->modes & SAMPLE_ENVELOPE) {
            //ensure envelope isin a release state
            if (note->env < 4) {
                note->env = 4;
            }
        
            // make sure this is set
            note->env_inc = -note->sample->env_rate[note->env];
        
            release = note->env_level / -note->env_inc;
        } else {
            // Sample release
            if (note->modes & SAMPLE_LOOP) {
                note->modes ^= SAMPLE_LOOP;
            }
            release = note->sample->data_length - note->sample_pos;
        }
        
        if (release > longest_release) longest_release = release;
        note->replay = NULL;
        note = note->next;
    }
    
    mdi->samples_to_mix = longest_release;
    
    return;
}

void _WM_do_meta_endoftrack(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record eot in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif

    _WM_Release_Allowance(mdi);
    return;
}

void _WM_do_meta_tempo(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_timesignature(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_keysignature(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_sequenceno(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_channelprefix(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_portprefix(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_smpteoffset(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_DEBUG(__FUNCTION__, ch, data->data.value);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_text(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_SDEBUG(__FUNCTION__, ch, data->data.string);
#endif
    if (mdi->extra_info.mixer_options & WM_MO_TEXTASLYRIC) {
        mdi->lyric = data->data.string;
    }

    return;
}

void _WM_do_meta_copyright(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_SDEBUG(__FUNCTION__, ch, data->data.string);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_trackname(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_SDEBUG(__FUNCTION__, ch, data->data.string);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_instrumentname(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_SDEBUG(__FUNCTION__, ch, data->data.string);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_lyric(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_SDEBUG(__FUNCTION__, ch, data->data.string);
#endif
    if (!(mdi->extra_info.mixer_options & WM_MO_TEXTASLYRIC)) {
        mdi->lyric = data->data.string;
    }
    return;
}

void _WM_do_meta_marker(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_SDEBUG(__FUNCTION__, ch, data->data.string);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_do_meta_cuepoint(struct _mdi *mdi, struct _event_data *data) {
/* placeholder function so we can record tempo in the event stream
 * for conversion function _WM_Event2Midi */
#ifdef DEBUG_MIDI
    uint8_t ch = data->channel;
    MIDI_EVENT_SDEBUG(__FUNCTION__, ch, data->data.string);
#else
    UNUSED(data);
#endif
    UNUSED(mdi);
    return;
}

void _WM_ResetToStart(struct _mdi *mdi) {
    struct _event * event = NULL;

    mdi->current_event = mdi->events;
    mdi->samples_to_mix = 0;
    mdi->extra_info.current_sample = 0;

    _WM_do_sysex_gm_reset(mdi, NULL);

    /* Ensure last event is NULL */
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = NULL;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = 0;
    mdi->events[mdi->event_count].samples_to_next = 0;

    if (_WM_MixerOptions & WM_MO_STRIPSILENCE) {
        event = mdi->events;
        /* Scan for first note on removing any samples as we go */
        if (event->do_event != *_WM_do_note_on) {
            do {
                if (event->samples_to_next != 0) {
                    mdi->extra_info.approx_total_samples -= event->samples_to_next;
                    event->samples_to_next = 0;
                }
                event++;
                if (event == NULL) break;
            } while (event->do_event != *_WM_do_note_on);
        }

        /* Reverse scan for last note off removing any samples as we go */
        event = &mdi->events[mdi->event_count - 1];
        if (event->do_event != *_WM_do_note_off) {
            do {
                mdi->extra_info.approx_total_samples -= event->samples_to_next;
                event->samples_to_next = 0;
                if (event == mdi->events) break; /* just to be safe */
                event--;
            } while (event->do_event != *_WM_do_note_off);
        }
        mdi->extra_info.approx_total_samples -= event->samples_to_next;
        event->samples_to_next = 0;
    }
}

int _WM_midi_setup_divisions(struct _mdi *mdi, uint32_t divisions) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0,0);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_midi_divisions;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = divisions;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

int _WM_midi_setup_noteoff(struct _mdi *mdi, uint8_t channel,
                           uint8_t note, uint8_t velocity) {
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, note);
    _WM_CheckEventMemoryPool(mdi);
    note &= 0x7f; /* silently bound note to 0..127 (github bug #180) */
    mdi->events[mdi->event_count].do_event = *_WM_do_note_off;
    mdi->events[mdi->event_count].event_data.channel = channel;
    mdi->events[mdi->event_count].event_data.data.value = (note << 8) | velocity;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_noteon(struct _mdi *mdi, uint8_t channel,
                             uint8_t note, uint8_t velocity) {
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, note);
    _WM_CheckEventMemoryPool(mdi);
    note &= 0x7f; /* silently bound note to 0..127 (github bug #180) */
    mdi->events[mdi->event_count].do_event = *_WM_do_note_on;
    mdi->events[mdi->event_count].event_data.channel = channel;
    mdi->events[mdi->event_count].event_data.data.value = (note << 8) | velocity;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;

    if (mdi->channel[channel].isdrum)
        _WM_load_patch(mdi, ((mdi->channel[channel].bank << 8) | (note | 0x80)));
    return (0);
}

static int midi_setup_aftertouch(struct _mdi *mdi, uint8_t channel,
                                 uint8_t note, uint8_t pressure) {
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, note);
    _WM_CheckEventMemoryPool(mdi);
    note &= 0x7f; /* silently bound note to 0..127 (github bug #180) */
    mdi->events[mdi->event_count].do_event = *_WM_do_aftertouch;
    mdi->events[mdi->event_count].event_data.channel = channel;
    mdi->events[mdi->event_count].event_data.data.value = (note << 8) | pressure;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_control(struct _mdi *mdi, uint8_t channel,
                              uint8_t controller, uint8_t setting) {
    void (*tmp_event)(struct _mdi *mdi, struct _event_data *data) = NULL;
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, controller);

    switch (controller) {
        /*
         **********************************************************************
         FIXME: Need to add dummy events for MIDI events we don't support.
         There is no reason not to store unsupported events in light of our
         out to midi option.
         **********************************************************************
         */
        case 0:
            tmp_event = *_WM_do_control_bank_select;
            mdi->channel[channel].bank = setting;
            break;
        case 6:
            tmp_event = *_WM_do_control_data_entry_course;
            break;
        case 7:
            tmp_event = *_WM_do_control_channel_volume;
            mdi->channel[channel].volume = setting;
            break;
        case 8:
            tmp_event = *_WM_do_control_channel_balance;
            break;
        case 10:
            tmp_event = *_WM_do_control_channel_pan;
            break;
        case 11:
            tmp_event = *_WM_do_control_channel_expression;
            break;
        case 38:
            tmp_event = *_WM_do_control_data_entry_fine;
            break;
        case 64:
            tmp_event = *_WM_do_control_channel_hold;
            break;
        case 96:
            tmp_event = *_WM_do_control_data_increment;
            break;
        case 97:
            tmp_event = *_WM_do_control_data_decrement;
            break;
        case 98:
            tmp_event = *_WM_do_control_non_registered_param_fine;
            break;
        case 99:
            tmp_event = *_WM_do_control_non_registered_param_course;
            break;
        case 100:
            tmp_event = *_WM_do_control_registered_param_fine;
            break;
        case 101:
            tmp_event = *_WM_do_control_registered_param_course;
            break;
        case 120:
            tmp_event = *_WM_do_control_channel_sound_off;
            break;
        case 121:
            tmp_event = *_WM_do_control_channel_controllers_off;
            break;
        case 123:
            tmp_event = *_WM_do_control_channel_notes_off;
            break;
        default:
            tmp_event = *_WM_do_control_dummy;
            break;
    }

    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = tmp_event;
    mdi->events[mdi->event_count].event_data.channel = channel;
    if (tmp_event != *_WM_do_control_dummy) {
        mdi->events[mdi->event_count].event_data.data.value = setting;
    } else {
        mdi->events[mdi->event_count].event_data.data.value = (controller << 8) | setting;
    }
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_patch(struct _mdi *mdi, uint8_t channel, uint8_t patch) {
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, patch);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_patch;
    mdi->events[mdi->event_count].event_data.channel = channel;
    mdi->events[mdi->event_count].event_data.data.value = patch;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;

    if (mdi->channel[channel].isdrum) {
        mdi->channel[channel].bank = patch;
    } else {
        _WM_load_patch(mdi, ((mdi->channel[channel].bank << 8) | patch));
        mdi->channel[channel].patch = _WM_get_patch_data(mdi,
                                                     ((mdi->channel[channel].bank << 8) | patch));
    }
    return (0);
}

static int midi_setup_channel_pressure(struct _mdi *mdi, uint8_t channel,
                                       uint8_t pressure) {
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, pressure);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_channel_pressure;
    mdi->events[mdi->event_count].event_data.channel = channel;
    mdi->events[mdi->event_count].event_data.data.value = pressure;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_pitch(struct _mdi *mdi, uint8_t channel, uint16_t pitch) {
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, pitch);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_pitch;
    mdi->events[mdi->event_count].event_data.channel = channel;
    mdi->events[mdi->event_count].event_data.data.value = pitch;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_sysex_roland_drum_track(struct _mdi *mdi,
                                              uint8_t channel, uint16_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,channel, setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = _WM_do_sysex_roland_drum_track;
    mdi->events[mdi->event_count].event_data.channel = channel;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;

    if (setting > 0) {
        mdi->channel[channel].isdrum = 1;
    } else {
        mdi->channel[channel].isdrum = 0;
    }
    return (0);
}

static int midi_setup_sysex_gm_reset(struct _mdi *mdi) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0,0);

    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_sysex_roland_reset;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = 0;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_sysex_roland_reset(struct _mdi *mdi) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0,0);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_sysex_roland_reset;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = 0;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_sysex_yamaha_reset(struct _mdi *mdi) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0,0);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_sysex_roland_reset;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = 0;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

int _WM_midi_setup_endoftrack(struct _mdi *mdi) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0,0);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_endoftrack;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = 0;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

int _WM_midi_setup_tempo(struct _mdi *mdi, uint32_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0,setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_tempo;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_timesignature(struct _mdi *mdi, uint32_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0, setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_timesignature;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_keysignature(struct _mdi *mdi, uint32_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0, setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_keysignature;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_sequenceno(struct _mdi *mdi, uint32_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0, setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_sequenceno;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_channelprefix(struct _mdi *mdi, uint32_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0, setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_channelprefix;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_portprefix(struct _mdi *mdi, uint32_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0, setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_portprefix;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_smpteoffset(struct _mdi *mdi, uint32_t setting) {
    MIDI_EVENT_DEBUG(__FUNCTION__,0, setting);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_smpteoffset;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.value = setting;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static void strip_text(char * text) {
    char * ch_loc = NULL;

    ch_loc = strrchr(text, '\n');
    while (ch_loc != NULL) {
        *ch_loc = ' ';
        ch_loc = strrchr(text, '\n');
    }
    ch_loc = strrchr(text, '\r');
    while (ch_loc != NULL) {
        *ch_loc = ' ';
        ch_loc = strrchr(text, '\r');
    }
}

static int midi_setup_text(struct _mdi *mdi, char * text) {
    MIDI_EVENT_SDEBUG(__FUNCTION__,0, text);
    strip_text(text);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_text;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.string = text;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_copyright(struct _mdi *mdi, char * text) {
    MIDI_EVENT_SDEBUG(__FUNCTION__,0, text);
    strip_text(text);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_copyright;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.string = text;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_trackname(struct _mdi *mdi, char * text) {
    MIDI_EVENT_SDEBUG(__FUNCTION__,0, text);
    strip_text(text);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_trackname;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.string = text;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_instrumentname(struct _mdi *mdi, char * text) {
    MIDI_EVENT_SDEBUG(__FUNCTION__,0, text);
    strip_text(text);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_instrumentname;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.string = text;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_lyric(struct _mdi *mdi, char * text) {
    MIDI_EVENT_SDEBUG(__FUNCTION__,0, text);
    strip_text(text);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_lyric;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.string = text;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_marker(struct _mdi *mdi, char * text) {
    MIDI_EVENT_SDEBUG(__FUNCTION__,0, text);
    strip_text(text);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_marker;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.string = text;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

static int midi_setup_cuepoint(struct _mdi *mdi, char * text) {
    MIDI_EVENT_SDEBUG(__FUNCTION__,0, text);
    strip_text(text);
    _WM_CheckEventMemoryPool(mdi);
    mdi->events[mdi->event_count].do_event = *_WM_do_meta_cuepoint;
    mdi->events[mdi->event_count].event_data.channel = 0;
    mdi->events[mdi->event_count].event_data.data.string = text;
    mdi->events[mdi->event_count].samples_to_next = 0;
    mdi->event_count++;
    return (0);
}

struct _mdi *
_WM_initMDI(void) {
    struct _mdi *mdi;

    mdi = (struct _mdi *) malloc(sizeof(struct _mdi));
    memset(mdi, 0, (sizeof(struct _mdi)));

    mdi->extra_info.copyright = NULL;
    mdi->extra_info.mixer_options = _WM_MixerOptions;

    _WM_load_patch(mdi, 0x0000);

    mdi->events_size = MEM_CHUNK;
    mdi->events = (struct _event *) malloc(mdi->events_size * sizeof(struct _event));
    mdi->event_count = 0;
    mdi->current_event = mdi->events;

    mdi->samples_to_mix = 0;
    mdi->extra_info.current_sample = 0;
    mdi->extra_info.total_midi_time = 0;
    mdi->extra_info.approx_total_samples = 0;

    mdi->dyn_vol = 1.0;
    mdi->dyn_vol_adjust = 0.0;
    mdi->dyn_vol_peak = 0;
    mdi->dyn_vol_to_reach = 1.0;

    mdi->is_type2 = 0;

    mdi->lyric = NULL;

    _WM_do_sysex_gm_reset(mdi, NULL);

    return (mdi);
}

void _WM_freeMDI(struct _mdi *mdi) {
    struct _sample *tmp_sample;
    uint32_t i;

    if (mdi->patch_count != 0) {
        _WM_Lock(&_WM_patch_lock);
        for (i = 0; i < mdi->patch_count; i++) {
            mdi->patches[i]->inuse_count--;
            if (mdi->patches[i]->inuse_count == 0) {
                /* free samples here */
                while (mdi->patches[i]->first_sample) {
                    tmp_sample = mdi->patches[i]->first_sample->next;
                    free(mdi->patches[i]->first_sample->data);
                    free(mdi->patches[i]->first_sample);
                    mdi->patches[i]->first_sample = tmp_sample;
                }
                mdi->patches[i]->loaded = 0;
            }
        }
        _WM_Unlock(&_WM_patch_lock);
        free(mdi->patches);
    }

    if (mdi->event_count != 0) {
        for (i = 0; i < mdi->event_count; i++) {
            /* Free up the string event storage */
            if (mdi->events[i].do_event == _WM_do_meta_text) {
                free(mdi->events[i].event_data.data.string);
            } else if (mdi->events[i].do_event == _WM_do_meta_text) {
                free(mdi->events[i].event_data.data.string);
            } else if (mdi->events[i].do_event == _WM_do_meta_copyright) {
                free(mdi->events[i].event_data.data.string);
            } else if (mdi->events[i].do_event == _WM_do_meta_trackname) {
                free(mdi->events[i].event_data.data.string);
            } else if (mdi->events[i].do_event == _WM_do_meta_instrumentname) {
                free(mdi->events[i].event_data.data.string);
            } else if (mdi->events[i].do_event == _WM_do_meta_lyric) {
                free(mdi->events[i].event_data.data.string);
            } else if (mdi->events[i].do_event == _WM_do_meta_marker) {
                free(mdi->events[i].event_data.data.string);
            } else if (mdi->events[i].do_event == _WM_do_meta_cuepoint) {
                free(mdi->events[i].event_data.data.string);
            }
        }
    }

    free(mdi->events);
    _WM_free_reverb(mdi->reverb);
    free(mdi->mix_buffer);
    free(mdi);
}

uint32_t _WM_SetupMidiEvent(struct _mdi *mdi, uint8_t * event_data, uint32_t input_length, uint8_t running_event) {
    /*
     Only add standard MIDI and Sysex events in here.
     Non-standard events need to be handled by calling function
     to avoid compatibility issues.

     TODO:
     Add value limit checks
     */
    uint32_t ret_cnt = 0;
    uint8_t command = 0;
    uint8_t channel = 0;
    uint8_t data_1 = 0;
    uint8_t data_2 = 0;
    char *text = NULL;

    if (!input_length) goto shortbuf;

    if (event_data[0] >= 0x80) {
        command = *event_data & 0xf0;
        channel = *event_data++ & 0x0f;
        ret_cnt++;
        if (--input_length == 0) goto shortbuf;
    } else {
        command = running_event & 0xf0;
        channel = running_event & 0x0f;
    }

    switch(command) {
        case 0x80:
        _SETUP_NOTEOFF:
            if (input_length < 2) goto shortbuf;
            data_1 = *event_data++;
            data_2 = *event_data++;
            _WM_midi_setup_noteoff(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0x90:
            if (event_data[1] == 0) goto _SETUP_NOTEOFF; /* A velocity of 0 in a note on is actually a note off */
            if (input_length < 2) goto shortbuf;
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_noteon(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0xa0:
            if (input_length < 2) goto shortbuf;
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_aftertouch(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0xb0:
            if (input_length < 2) goto shortbuf;
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_control(mdi, channel, data_1, data_2);
            ret_cnt += 2;
            break;
        case 0xc0:
            data_1 = *event_data++;
            midi_setup_patch(mdi, channel, data_1);
            ret_cnt++;
            break;
        case 0xd0:
            data_1 = *event_data++;
            midi_setup_channel_pressure(mdi, channel, data_1);
            ret_cnt++;
            break;
        case 0xe0:
            if (input_length < 2) goto shortbuf;
            data_1 = *event_data++;
            data_2 = *event_data++;
            midi_setup_pitch(mdi, channel, ((data_2 << 7) | (data_1 & 0x7f)));
            ret_cnt += 2;
            break;
        case 0xf0:
            if (channel == 0x0f) {
                /*
                 MIDI Meta Events
                 */
                uint32_t tmp_length = 0;
                if ((event_data[0] == 0x00) && (event_data[1] == 0x02)) {
                    /*
                     Sequence Number
                     We only setting this up here for WM_Event2Midi function
                     */
                    if (input_length < 4) goto shortbuf;
                    midi_setup_sequenceno(mdi, ((event_data[2] << 8) + event_data[3]));
                    ret_cnt += 4;
                } else if (event_data[0] == 0x01) {
                    /* Text Event */
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    if (--input_length < tmp_length) goto shortbuf;
                    if (!tmp_length) break;/* broken file? */

                    text = (char *) malloc(tmp_length + 1);
                    memcpy(text, event_data, tmp_length);
                    text[tmp_length] = '\0';
                    midi_setup_text(mdi, text);

                    ret_cnt += tmp_length;

                } else if (event_data[0] == 0x02) {
                    /* Copyright Event */
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    if (--input_length < tmp_length) goto shortbuf;
                    if (!tmp_length) break;/* broken file? */

                    /* Copy copyright info in the getinfo struct */
                    if (mdi->extra_info.copyright) {
                        mdi->extra_info.copyright = (char *) realloc(mdi->extra_info.copyright,(strlen(mdi->extra_info.copyright) + 1 + tmp_length + 1));
                        memcpy(&mdi->extra_info.copyright[strlen(mdi->extra_info.copyright) + 1], event_data, tmp_length);
                        mdi->extra_info.copyright[strlen(mdi->extra_info.copyright) + 1 + tmp_length] = '\0';
                        mdi->extra_info.copyright[strlen(mdi->extra_info.copyright)] = '\n';
                    } else {
                        mdi->extra_info.copyright = (char *) malloc(tmp_length + 1);
                        memcpy(mdi->extra_info.copyright, event_data, tmp_length);
                        mdi->extra_info.copyright[tmp_length] = '\0';
                    }

                    /* NOTE: free'd when events are cleared during closure of mdi */
                    text = (char *) malloc(tmp_length + 1);
                    memcpy(text, event_data, tmp_length);
                    text[tmp_length] = '\0';
                    midi_setup_copyright(mdi, text);

                    ret_cnt += tmp_length;

                } else if (event_data[0] == 0x03) {
                    /* Track Name Event */
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    if (--input_length < tmp_length) goto shortbuf;
                    if (!tmp_length) break;/* broken file? */

                    text = (char *) malloc(tmp_length + 1);
                    memcpy(text, event_data, tmp_length);
                    text[tmp_length] = '\0';
                    midi_setup_trackname(mdi, text);

                    ret_cnt += tmp_length;

                } else if (event_data[0] == 0x04) {
                    /* Instrument Name Event */
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    if (--input_length < tmp_length) goto shortbuf;
                    if (!tmp_length) break;/* broken file? */

                    text = (char *) malloc(tmp_length + 1);
                    memcpy(text, event_data, tmp_length);
                    text[tmp_length] = '\0';
                    midi_setup_instrumentname(mdi, text);

                    ret_cnt += tmp_length;

                } else if (event_data[0] == 0x05) {
                    /* Lyric Event */
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    if (--input_length < tmp_length) goto shortbuf;
                    if (!tmp_length) break;/* broken file? */

                    text = (char *) malloc(tmp_length + 1);
                    memcpy(text, event_data, tmp_length);
                    text[tmp_length] = '\0';
                    midi_setup_lyric(mdi, text);

                    ret_cnt += tmp_length;

                } else if (event_data[0] == 0x06) {
                    /* Marker Event */
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    if (--input_length < tmp_length) goto shortbuf;
                    if (!tmp_length) break;/* broken file? */

                    text = (char *) malloc(tmp_length + 1);
                    memcpy(text, event_data, tmp_length);
                    text[tmp_length] = '\0';
                    midi_setup_marker(mdi, text);

                    ret_cnt += tmp_length;

                } else if (event_data[0] == 0x07) {
                    /* Cue Point Event */
                    /* Get Length */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    event_data++;
                    ret_cnt++;
                    if (--input_length < tmp_length) goto shortbuf;
                    if (!tmp_length) break;/* broken file? */

                    text = (char *) malloc(tmp_length + 1);
                    memcpy(text, event_data, tmp_length);
                    text[tmp_length] = '\0';
                    midi_setup_cuepoint(mdi, text);

                    ret_cnt += tmp_length;

                } else if ((event_data[0] == 0x20) && (event_data[1] == 0x01)) {
                    /*
                     Channel Prefix
                     We only setting this up here for WM_Event2Midi function
                     */
                    if (input_length < 3) goto shortbuf;
                    midi_setup_channelprefix(mdi, event_data[2]);
                    ret_cnt += 3;
                } else if ((event_data[0] == 0x21) && (event_data[1] == 0x01)) {
                    /*
                     Port Prefix
                     We only setting this up here for WM_Event2Midi function
                     */
                    if (input_length < 3) goto shortbuf;
                    midi_setup_portprefix(mdi, event_data[2]);
                    ret_cnt += 3;
                } else if ((event_data[0] == 0x2F) && (event_data[1] == 0x00)) {
                    /*
                     End of Track
                     Deal with this inside calling function
                     We only setting this up here for _WM_Event2Midi function
                     */
                    if (input_length < 2) goto shortbuf;
                    _WM_midi_setup_endoftrack(mdi);
                    ret_cnt += 2;
                } else if ((event_data[0] == 0x51) && (event_data[1] == 0x03)) {
                    /*
                     Tempo
                     Deal with this inside calling function.
                     We only setting this up here for _WM_Event2Midi function
                     */
                    if (input_length < 5) goto shortbuf;
                    _WM_midi_setup_tempo(mdi, ((event_data[2] << 16) + (event_data[3] << 8) + event_data[4]));
                    ret_cnt += 5;
                } else if ((event_data[0] == 0x54) && (event_data[1] == 0x05)) {
                    if (input_length < 7) goto shortbuf;
                    /*
                     SMPTE Offset
                     We only setting this up here for WM_Event2Midi function
                     */
                    midi_setup_smpteoffset(mdi, ((event_data[3] << 24) + (event_data[4] << 16) + (event_data[5] << 8) + event_data[6]));

                    /*
                     Because this has 5 bytes of data we gonna "hack" it a little
                     */
                    mdi->events[mdi->events_size - 1].event_data.channel = event_data[2];

                    ret_cnt += 7;
                } else if ((event_data[0] == 0x58) && (event_data[1] == 0x04)) {
                    /*
                     Time Signature
                     We only setting this up here for WM_Event2Midi function
                     */
                    if (input_length < 6) goto shortbuf;
                    midi_setup_timesignature(mdi, ((event_data[2] << 24) + (event_data[3] << 16) + (event_data[4] << 8) + event_data[5]));
                    ret_cnt += 6;
                } else if ((event_data[0] == 0x59) && (event_data[1] == 0x02)) {
                    /*
                     Key Signature
                     We only setting this up here for WM_Event2Midi function
                     */
                    if (input_length < 4) goto shortbuf;
                    midi_setup_keysignature(mdi, ((event_data[2] << 8) + event_data[3]));
                    ret_cnt += 4;
                } else {
                    /*
                     Unsupported Meta Event
                     */
                    event_data++;
                    ret_cnt++;
                    if (--input_length && *event_data > 0x7f) {
                        do {
                            if (!input_length) break;
                            tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                            event_data++;
                            input_length--;
                            ret_cnt++;
                        } while (*event_data > 0x7f);
                    }
                    if (!input_length) goto shortbuf;
                    tmp_length = (tmp_length << 7) + (*event_data & 0x7f);
                    ret_cnt++;
                    ret_cnt += tmp_length;
                    if (--input_length < tmp_length) goto shortbuf;
                }

            } else if ((channel == 0) || (channel == 7)) {
                /*
                 Sysex Events
                 */
                uint32_t sysex_len = 0;
                uint8_t *sysex_store = NULL;

                if (*event_data > 0x7f) {
                    do {
                        if (!input_length) break;
                        sysex_len = (sysex_len << 7) + (*event_data & 0x7F);
                        event_data++;
                        input_length--;
                        ret_cnt++;
                    } while (*event_data > 0x7f);
                }
                if (!input_length) goto shortbuf;
                sysex_len = (sysex_len << 7) + (*event_data & 0x7F);
                event_data++;
                ret_cnt++;
                if (--input_length < sysex_len) goto shortbuf;
                if (!sysex_len) break;/* broken file? */

                sysex_store = (uint8_t *) malloc(sizeof(uint8_t) * sysex_len);
                memcpy(sysex_store, event_data, sysex_len);

                if (sysex_store[sysex_len - 1] == 0xF7) {
                    uint8_t rolandsysexid[] = { 0x41, 0x10, 0x42, 0x12 };
                    if (memcmp(rolandsysexid, sysex_store, 4) == 0) {
                        /* For Roland Sysex Messages */
                        /* checksum */
                        uint8_t sysex_cs = 0;
                        uint32_t sysex_ofs = 4;
                        do {
                            sysex_cs += sysex_store[sysex_ofs];
                            if (sysex_cs > 0x7F) {
                                sysex_cs -= 0x80;
                            }
                            sysex_ofs++;
                        } while (sysex_store[sysex_ofs + 1] != 0xf7);
                        sysex_cs = 128 - sysex_cs;
                        /* is roland sysex message valid */
                        if (sysex_cs == sysex_store[sysex_ofs]) {
                            /* process roland sysex event */
                            if (sysex_store[4] == 0x40) {
                                if (((sysex_store[5] & 0xf0) == 0x10) && (sysex_store[6] == 0x15)) {
                                    /* Roland Drum Track Setting */
                                    uint8_t sysex_ch = 0x0f & sysex_store[5];
                                    if (sysex_ch == 0x00) {
                                        sysex_ch = 0x09;
                                    } else if (sysex_ch <= 0x09) {
                                        sysex_ch -= 1;
                                    }
                                    midi_setup_sysex_roland_drum_track(mdi, sysex_ch, sysex_store[7]);
                                } else if ((sysex_store[5] == 0x00) && (sysex_store[6] == 0x7F) && (sysex_store[7] == 0x00)) {
                                    /* Roland GS Reset */
                                    midi_setup_sysex_roland_reset(mdi);
                                }
                            }
                        }
                    } else {
                        /* For non-Roland Sysex Messages */
                        uint8_t gm_reset[] = {0x7e, 0x7f, 0x09, 0x01, 0xf7};
                        uint8_t yamaha_reset[] = {0x43, 0x10, 0x4c, 0x00, 0x00, 0x7e, 0x00, 0xf7};

                        if (memcmp(gm_reset, sysex_store, 5) == 0) {
                            /* GM Reset */
                            midi_setup_sysex_gm_reset(mdi);
                        } else if (memcmp(yamaha_reset,sysex_store,8) == 0) {
                            /* Yamaha Reset */
                            midi_setup_sysex_yamaha_reset(mdi);
                        }
                    }
                }
                free(sysex_store);
                sysex_store = NULL;
                /*
                event_data += sysex_len;
                */
                ret_cnt += sysex_len;
            } else {
                _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(unrecognized meta type event)", 0);
                return 0;
            }
            break;

        default: /* Should NEVER get here */
            ret_cnt = 0;
            break;
    }
    if (ret_cnt == 0)
        _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(missing event)", 0);
    return ret_cnt;

shortbuf:
    _WM_GLOBAL_ERROR(__FUNCTION__, __LINE__, WM_ERR_CORUPT, "(input too short)", 0);
    return 0;
}

