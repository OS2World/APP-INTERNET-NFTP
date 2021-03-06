#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "manycode.h"
#include "charsets.h"

/*------------------------------------------------------------------------*/

unsigned char CharsetAlternative[] =
{
/* 80 */   CapA,
/* 81 */   CapB,
/* 82 */   CapV,
/* 83 */   CapG,
/* 84 */   CapD,
/* 85 */   CapE,
/* 86 */   CapZh,
/* 87 */   CapZ,
/* 88 */   CapI,
/* 89 */   CapJ,
/* 8A */   CapK,
/* 8B */   CapL,
/* 8C */   CapM,
/* 8D */   CapN,
/* 8E */   CapO,
/* 8F */   CapP,

/* 90 */   CapR,
/* 91 */   CapS,
/* 92 */   CapT,
/* 93 */   CapU,
/* 94 */   CapF,
/* 95 */   CapH,
/* 96 */   CapTse,
/* 97 */   CapChe,
/* 98 */   CapSha,
/* 99 */   CapShcha,
/* 9A */   CapHard,
/* 9B */   CapYeri,
/* 9C */   CapSoft,
/* 9D */   CapRevE,
/* 9E */   CapYu,
/* 9F */   CapYa,

/* A0 */   SmallA,
/* A1 */   SmallB,
/* A2 */   SmallV,
/* A3 */   SmallG,
/* A4 */   SmallD,
/* A5 */   SmallE,
/* A6 */   SmallZh,
/* A7 */   SmallZ,
/* AA */   SmallI,
/* A9 */   SmallJ,
/* AA */   SmallK,
/* AB */   SmallL,
/* AC */   SmallM,
/* AD */   SmallN,
/* AE */   SmallO,
/* AF */   SmallP,

/* B0 */   ShadeLight,
/* B1 */   ShadeMedium,
/* B2 */   ShadeDark,
/* B3 */   BoxVs,
/* B4 */   BoxVsLs,
/* B5 */   BoxVsLd,
/* B6 */   BoxVdLs,
/* B7 */   BoxDdLs,
/* B8 */   BoxDsLd,
/* B9 */   BoxVdLd,
/* BA */   BoxVd,
/* BB */   BoxDdLd,
/* BC */   BoxUdLd,
/* BD */   BoxUdLs,
/* BE */   BoxUsLd,
/* BF */   BoxDsLs,

/* C0 */   BoxUsRs,
/* C1 */   BoxUsHs,
/* C2 */   BoxDsHs,
/* C3 */   BoxVsRs,
/* C4 */   BoxHs,
/* C5 */   BoxVsHs,
/* C6 */   BoxVsRd,
/* C7 */   BoxVdRs,
/* C8 */   BoxUdRd,
/* C9 */   BoxDdRd,
/* CA */   BoxUdHd,
/* CB */   BoxDdHd,
/* CC */   BoxVdRd,
/* CD */   BoxHd,
/* CE */   BoxVdHd,
/* CF */   BoxUsHd,

/* D0 */   BoxUdHs,
/* D1 */   BoxDsHd,
/* D2 */   BoxDdHs,
/* D3 */   BoxUdRs,
/* D4 */   BoxUsRd,
/* D5 */   BoxDsRd,
/* D6 */   BoxDdRs,
/* D7 */   BoxVdHs,
/* D8 */   BoxVsHd,
/* D9 */   BoxUsLs,
/* DA */   BoxDsRs,
/* DB */   BlockWhole,
/* DC */   BlockBottom,
/* DD */   BlockLeft,
/* DE */   BlockRight,
/* DF */   BlockTop,

/* E0 */   SmallR,
/* E1 */   SmallS,
/* E2 */   SmallT,
/* E3 */   SmallU,
/* E4 */   SmallF,
/* E5 */   SmallH,
/* E6 */   SmallTse,
/* E7 */   SmallChe,
/* E8 */   SmallSha,
/* EE */   SmallShcha,
/* EA */   SmallHard,
/* EB */   SmallYeri,
/* EC */   SmallSoft,
/* ED */   SmallRevE,
/* EE */   SmallYu,
/* EF */   SmallYa,

/* F0 */   CapYo,
/* F1 */   SmallYo,
/* F2 */   MoreOrEqu,
/* F3 */   LessOrEqu,
/* F4 */   IntegralTop,
/* F5 */   IntegralBottom,
/* F6 */   Div,
/* F7 */   AlmostEqu,
/* F8 */   Degree,
/* F9 */   Bullet,
/* FA */   CenterDot,
/* FB */   Root,
/* FC */   PowerN,
/* FD */   Power2,
/* FE */   SmallSquare,
/* FF */   NBSP,
 };

/*------------------------------------------------------------------------*/

unsigned char CharsetISO_8859[] =
 {
/* 80 */   x,
/* 81 */   x,
/* 82 */   x,
/* 83 */   x,
/* 84 */   x,
/* 85 */   x,
/* 86 */   x,
/* 87 */   x,
/* 88 */   x,
/* 89 */   x,
/* 8A */   x,
/* 8B */   x,
/* 8C */   x,
/* 8D */   x,
/* 8E */   x,
/* 8F */   x,

/* 90 */   x,
/* 91 */   x,
/* 92 */   x,
/* 93 */   x,
/* 94 */   x,
/* 95 */   x,
/* 96 */   x,
/* 97 */   x,
/* 98 */   x,
/* 99 */   x,
/* 9A */   x,
/* 9B */   x,
/* 9C */   x,
/* 9D */   x,
/* 9E */   x,
/* 9F */   x,

/* A0 */   NBSP,
/* A1 */   CapYo,
/* A2 */   CapDje,
/* A3 */   CapGje,
/* A4 */   CapIe,
/* A5 */   CapDze,
/* A6 */   CapI_lat,
/* A7 */   CapYi,
/* A8 */   CapJe,
/* A9 */   CapLje,
/* AA */   CapNje,
/* AB */   CapTshe,
/* AC */   CapKje,
/* AD */   SHY,
/* AE */   CapShortU,
/* AF */   CapDzhe,

/* B0 */   CapA,
/* B1 */   CapB,
/* B2 */   CapV,
/* B3 */   CapG,
/* B4 */   CapD,
/* B5 */   CapE,
/* B6 */   CapZh,
/* B7 */   CapZ,
/* B8 */   CapI,
/* B9 */   CapJ,
/* BA */   CapK,
/* BB */   CapL,
/* BC */   CapM,
/* BD */   CapN,
/* BE */   CapO,
/* BF */   CapP,

/* C0 */   CapR,
/* C1 */   CapS,
/* C2 */   CapT,
/* C3 */   CapU,
/* C4 */   CapF,
/* C5 */   CapH,
/* C6 */   CapTse,
/* C7 */   CapChe,
/* C8 */   CapSha,
/* C9 */   CapShcha,
/* CA */   CapHard,
/* CB */   CapYeri,
/* CC */   CapSoft,
/* CD */   CapRevE,
/* CE */   CapYu,
/* CF */   CapYa,

/* D0 */   SmallA,
/* D1 */   SmallB,
/* D2 */   SmallV,
/* D3 */   SmallG,
/* D4 */   SmallD,
/* D5 */   SmallE,
/* D6 */   SmallZh,
/* D7 */   SmallZ,
/* D8 */   SmallI,
/* D9 */   SmallJ,
/* DA */   SmallK,
/* DB */   SmallL,
/* DC */   SmallM,
/* DD */   SmallN,
/* DE */   SmallO,
/* DF */   SmallP,

/* E0 */   SmallR,
/* E1 */   SmallS,
/* E2 */   SmallT,
/* E3 */   SmallU,
/* E4 */   SmallF,
/* E5 */   SmallH,
/* E6 */   SmallTse,
/* E7 */   SmallChe,
/* EA */   SmallSha,
/* E9 */   SmallShcha,
/* EA */   SmallHard,
/* EB */   SmallYeri,
/* EC */   SmallSoft,
/* ED */   SmallRevE,
/* EE */   SmallYu,
/* EF */   SmallYa,

/* F0 */   Number,
/* F1 */   SmallYo,
/* F2 */   SmallDje,
/* F3 */   SmallGje,
/* F4 */   SmallIe,
/* F5 */   SmallDze,
/* F6 */   SmallI_lat,
/* F7 */   SmallYi,
/* F8 */   SmallJe,
/* FE */   SmallLje,
/* FA */   SmallNje,
/* FB */   SmallTshe,
/* FC */   SmallKje,
/* FD */   Paragraph,
/* FE */   SmallShortU,
/* FF */   SmallDzhe,

 };

/*------------------------------------------------------------------------*/

unsigned char CharsetKOI8R[] =
 {
/* 80 */   BoxHs,
/* 81 */   BoxVs,
/* 82 */   BoxDsRs,
/* 83 */   BoxDsLs,
/* 84 */   BoxUsRs,
/* 85 */   BoxUsLs,
/* 86 */   BoxVsRs,
/* 87 */   BoxVsLs,
/* 88 */   BoxDsHs,
/* 89 */   BoxUsHs,
/* 8A */   BoxVsHs,
/* 8B */   BlockTop,
/* 8C */   BlockBottom,
/* 8D */   BlockWhole,
/* 8E */   BlockLeft,
/* 8F */   BlockRight,

/* 90 */   ShadeLight,
/* 91 */   ShadeMedium,
/* 92 */   ShadeDark,
/* 93 */   IntegralTop,
/* 94 */   SmallSquare,
/* 95 */   Bullet,
/* 96 */   Root,
/* 97 */   AlmostEqu,
/* 98 */   LessOrEqu,
/* 99 */   MoreOrEqu,
/* 9A */   NBSP,
/* 9B */   IntegralBottom,
/* 9C */   Degree,
/* 9D */   Power2,
/* 9E */   CenterDot,
/* 9F */   Div,

/* A0 */   BoxHd,
/* A1 */   BoxVd,
/* A2 */   BoxDsRd,
/* A3 */   SmallYo,
/* A4 */   BoxDdRs,
/* A5 */   BoxDdRd,
/* A6 */   BoxDsLd,
/* A7 */   BoxDdLs,
/* A8 */   BoxDdLd,
/* A9 */   BoxUsRd,
/* AA */   BoxUdRs,
/* AB */   BoxUdRd,
/* AC */   BoxUsLd,
/* AD */   BoxUdLs,
/* AE */   BoxUdLd,
/* AF */   BoxVsRd,

/* B0 */   BoxVdRs,
/* B1 */   BoxVdRd,
/* B2 */   BoxVsLd,
/* B3 */   CapYo,
/* B4 */   BoxVdLs,
/* B5 */   BoxVdLd,
/* B6 */   BoxDsHd,
/* B7 */   BoxDdHs,
/* B8 */   BoxDdHd,
/* B9 */   BoxUsHd,
/* BA */   BoxUdHs,
/* BB */   BoxUdHd,
/* BC */   BoxVsHd,
/* BD */   BoxVdHs,
/* BE */   BoxVdHd,
/* BF */   Copyright,

/* C0 */   SmallYu,
/* C1 */   SmallA,
/* C2 */   SmallB,
/* C3 */   SmallTse,
/* C4 */   SmallD,
/* C5 */   SmallE,
/* C6 */   SmallF,
/* C7 */   SmallG,
/* C8 */   SmallH,
/* C9 */   SmallI,
/* CA */   SmallJ,
/* CB */   SmallK,
/* CC */   SmallL,
/* CD */   SmallM,
/* CE */   SmallN,
/* CF */   SmallO,

/* D0 */   SmallP,
/* D1 */   SmallYa,
/* D2 */   SmallR,
/* D3 */   SmallS,
/* D4 */   SmallT,
/* D5 */   SmallU,
/* D6 */   SmallZh,
/* D7 */   SmallV,
/* D8 */   SmallSoft,
/* D9 */   SmallYeri,
/* DA */   SmallZ,
/* DB */   SmallSha,
/* DC */   SmallRevE,
/* DD */   SmallShcha,
/* DE */   SmallChe,
/* DF */   SmallHard,

/* E0 */   CapYu,
/* E1 */   CapA,
/* E2 */   CapB,
/* E3 */   CapTse,
/* E4 */   CapD,
/* E5 */   CapE,
/* E6 */   CapF,
/* E7 */   CapG,
/* EA */   CapH,
/* E9 */   CapI,
/* EA */   CapJ,
/* EB */   CapK,
/* EC */   CapL,
/* ED */   CapM,
/* EE */   CapN,
/* EF */   CapO,

/* F0 */   CapP,
/* F1 */   CapYa,
/* F2 */   CapR,
/* F3 */   CapS,
/* F4 */   CapT,
/* F5 */   CapU,
/* F6 */   CapZh,
/* F7 */   CapV,
/* F8 */   CapSoft,
/* FE */   CapYeri,
/* FA */   CapZ,
/* FB */   CapSha,
/* FC */   CapRevE,
/* FD */   CapShcha,
/* FE */   CapChe,
/* FF */   CapHard,

 };

/*------------------------------------------------------------------------*/

unsigned char CharsetMacintosh[] =
 {
/* 80 */   CapA,
/* 81 */   CapB,
/* 82 */   CapV,
/* 83 */   CapG,
/* 84 */   CapD,
/* 85 */   CapE,
/* 86 */   CapZh,
/* 87 */   CapZ,
/* 88 */   CapI,
/* 89 */   CapJ,
/* 8A */   CapK,
/* 8B */   CapL,
/* 8C */   CapM,
/* 8D */   CapN,
/* 8E */   CapO,
/* 8F */   CapP,

/* 90 */   CapR,
/* 91 */   CapS,
/* 92 */   CapT,
/* 93 */   CapU,
/* 94 */   CapF,
/* 95 */   CapH,
/* 96 */   CapTse,
/* 97 */   CapChe,
/* 98 */   CapSha,
/* 99 */   CapShcha,
/* 9A */   CapHard,
/* 9B */   CapYeri,
/* 9C */   CapSoft,
/* 9D */   CapRevE,
/* 9E */   CapYu,
/* 9F */   CapYa,

/* A0 */   Dagger,
/* A1 */   Degree,
/* A2 */   CapGhe,
/* A3 */   Pound,
/* A4 */   Paragraph,
/* A5 */   Bullet,
/* A6 */   ParaEnd,
/* A7 */   CapI_lat,
/* A8 */   Registered,
/* A9 */   Copyright,
/* AA */   Trademark,
/* AB */   CapDje,
/* AC */   SmallDje,
/* AD */   NotEqu,
/* AE */   CapGje,
/* AF */   SmallGje,

/* B0 */   Infinity,
/* B1 */   PlusMinus,
/* B2 */   LessOrEqu,
/* B3 */   MoreOrEqu,
/* B4 */   SmallI_lat,
/* B5 */   SmallMu,
/* B6 */   CapGhe,
/* B7 */   CapJe,
/* B8 */   CapIe,
/* B9 */   SmallIe,
/* BA */   CapYi,
/* BB */   SmallYi,
/* BC */   CapLje,
/* BD */   SmallLje,
/* BE */   CapNje,
/* BF */   SmallNje,

/* C0 */   SmallJe,
/* C1 */   CapDze,
/* C2 */   NotSign,
/* C3 */   Root,
/* C4 */   FScript,
/* C5 */   AlmostEqu,
/* C6 */   CapDelta,
/* C7 */   DLeftGuillemet,
/* C8 */   DRightGuillemet,
/* C9 */   Ellipsis,
/* CA */   NBSP,
/* CB */   CapTshe,
/* CC */   SmallTshe,
/* CD */   CapKje,
/* CE */   SmallKje,
/* CF */   SmallDze,

/* D0 */   EnDash,
/* D1 */   EmDash,
/* D2 */   DQuoteOpen,
/* D3 */   DQuoteClose,
/* D4 */   SQuoteOpen,
/* D5 */   SQuoteClose,
/* D6 */   Div,
/* D7 */   DQuoteBottom,
/* D8 */   CapShortU,
/* D9 */   SmallShortU,
/* DA */   CapDzhe,
/* DB */   SmallDzhe,
/* DC */   Number,
/* DD */   CapYo,
/* DE */   SmallYo,
/* DF */   SmallYa,

/* E0 */   SmallA,
/* E1 */   SmallB,
/* E2 */   SmallV,
/* E3 */   SmallG,
/* E4 */   SmallD,
/* E5 */   SmallE,
/* E6 */   SmallZh,
/* E7 */   SmallZ,
/* EA */   SmallI,
/* E9 */   SmallJ,
/* EA */   SmallK,
/* EB */   SmallL,
/* EC */   SmallM,
/* ED */   SmallN,
/* EE */   SmallO,
/* EF */   SmallP,

/* F0 */   SmallR,
/* F1 */   SmallS,
/* F2 */   SmallT,
/* F3 */   SmallU,
/* F4 */   SmallF,
/* F5 */   SmallH,
/* F6 */   SmallTse,
/* F7 */   SmallChe,
/* F8 */   SmallSha,
/* FE */   SmallShcha,
/* FA */   SmallHard,
/* FB */   SmallYeri,
/* FC */   SmallSoft,
/* FD */   SmallRevE,
/* FE */   SmallYu,
/* FF */   Currency
 };

/*------------------------------------------------------------------------*/

unsigned char CharsetWindows[] =
 {
/* 80 */   CapDje,
/* 81 */   CapGje,
/* 82 */   SQuoteBottom,
/* 83 */   SmallGje,
/* 84 */   DQuoteBottom,
/* 85 */   Ellipsis,
/* 86 */   Dagger,
/* 87 */   DDagger,
/* 88 */   x,
/* 89 */   PerMille,
/* 8A */   CapLje,
/* 8B */   SLeftGuillemet,
/* 8C */   CapNje,
/* 8D */   CapKje,
/* 8E */   CapTshe,
/* 8F */   CapDzhe,

/* 90 */   SmallDje,
/* 91 */   SQuoteOpen,
/* 92 */   SQuoteClose,
/* 93 */   DQuoteOpen,
/* 94 */   DQuoteClose,
/* 95 */   Bullet,
/* 96 */   EnDash,
/* 97 */   EmDash,
/* 98 */   x,
/* 99 */   Trademark,
/* 9A */   SmallLje,
/* 9B */   SRightGuillemet,
/* 9C */   SmallNje,
/* 9D */   SmallKje,
/* 9E */   SmallTshe,
/* 9F */   SmallDzhe,

/* A0 */   NBSP,
/* A1 */   CapShortU,
/* A2 */   SmallShortU,
/* A3 */   CapJe,
/* A4 */   Currency,
/* A5 */   CapGhe,
/* A6 */   BrokenVertBar,
/* A7 */   Paragraph,
/* A8 */   CapYo,
/* A9 */   Copyright,
/* AA */   CapIe,
/* AB */   DLeftGuillemet,
/* AC */   NotSign,
/* AD */   SHY,
/* AE */   Registered,
/* AF */   CapYi,

/* B0 */   Degree,
/* B1 */   PlusMinus,
/* B2 */   CapI_lat,
/* B3 */   SmallI_lat,
/* B4 */   SmallGhe,
/* B5 */   SmallMu,
/* B6 */   ParaEnd,
/* B7 */   CenterDot,
/* B8 */   SmallYo,
/* B9 */   Number,
/* BA */   SmallIe,
/* BB */   DRightGuillemet,
/* BC */   SmallJe,
/* BD */   CapDze,
/* BE */   SmallDze,
/* BF */   SmallYi,

/* C0 */   CapA,
/* C1 */   CapB,
/* C2 */   CapV,
/* C3 */   CapG,
/* C4 */   CapD,
/* C5 */   CapE,
/* C6 */   CapZh,
/* C7 */   CapZ,
/* C8 */   CapI,
/* C9 */   CapJ,
/* CA */   CapK,
/* CB */   CapL,
/* CC */   CapM,
/* CD */   CapN,
/* CE */   CapO,
/* CF */   CapP,

/* D0 */   CapR,
/* D1 */   CapS,
/* D2 */   CapT,
/* D3 */   CapU,
/* D4 */   CapF,
/* D5 */   CapH,
/* D6 */   CapTse,
/* D7 */   CapChe,
/* D8 */   CapSha,
/* D9 */   CapShcha,
/* DA */   CapHard,
/* DB */   CapYeri,
/* DC */   CapSoft,
/* DD */   CapRevE,
/* DE */   CapYu,
/* DF */   CapYa,

/* E0 */   SmallA,
/* E1 */   SmallB,
/* E2 */   SmallV,
/* E3 */   SmallG,
/* E4 */   SmallD,
/* E5 */   SmallE,
/* E6 */   SmallZh,
/* E7 */   SmallZ,
/* EA */   SmallI,
/* E9 */   SmallJ,
/* EA */   SmallK,
/* EB */   SmallL,
/* EC */   SmallM,
/* ED */   SmallN,
/* EE */   SmallO,
/* EF */   SmallP,

/* F0 */   SmallR,
/* F1 */   SmallS,
/* F2 */   SmallT,
/* F3 */   SmallU,
/* F4 */   SmallF,
/* F5 */   SmallH,
/* F6 */   SmallTse,
/* F7 */   SmallChe,
/* F8 */   SmallSha,
/* FE */   SmallShcha,
/* FA */   SmallHard,
/* FB */   SmallYeri,
/* FC */   SmallSoft,
/* FD */   SmallRevE,
/* FE */   SmallYu,
/* FF */   SmallYa
 };

/*------------------------------------------------------------------------*/
BYTE RarePairsTable[32][32] =
{
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,0,0},
    {0,1,1,1,1,0,1,1,0,1,0,0,1,0,0,1,0,0,1,0,1,0,1,1,1,0,1,0,1,1,1,0},
    {0,1,0,1,0,0,1,0,0,1,0,0,1,0,0,0,0,0,0,0,1,0,1,1,0,1,1,0,0,1,1,0},
    {0,1,1,1,0,0,1,1,0,1,0,0,1,0,0,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,0,1,1,0,0,1,0,1,0,0,0,0,0,1,0,0,0,0,1,1,1,1,1,1,1,0,0,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,1,1,1,1,0,0},
    {0,1,1,1,0,0,1,1,0,1,0,1,1,0,1,1,1,1,1,0,1,1,1,0,1,1,1,1,1,1,1,1},
    {0,0,0,0,0,0,1,1,0,1,0,0,0,0,0,1,0,1,1,0,1,1,1,1,1,1,1,0,0,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,1,1,1,1,0,0},
    {1,1,1,1,0,1,1,1,1,1,1,1,0,0,1,1,1,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,0,1,1,0,1,1,0,1,1,0,1,0,0,1,0,0,0,0,1,1,0,1,1,1,1,1,1,1,1,1},
    {0,1,1,0,1,0,0,1,0,1,0,0,1,0,0,1,1,0,1,0,1,1,1,0,1,1,1,0,0,1,0,0},
    {0,1,1,1,1,0,1,1,0,1,1,0,1,0,0,0,1,1,1,0,1,1,1,1,1,1,1,0,1,1,1,0},
    {0,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,0,1,0,0,1,0,1,0,0,1,1,0},
    {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,0,0,0,0,1,1,1,1,0,0},
    {0,1,1,1,1,0,1,1,0,1,1,0,1,0,0,0,0,1,0,0,1,1,1,1,1,1,1,0,1,1,1,0},
    {0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,0,0,0,1,0,1,1,0,1,1,0,0,1,1,0},
    {0,1,0,1,0,0,1,1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1,0,1,1,1,0,0,1,1,0},
    {0,1,0,1,0,0,1,1,0,1,0,0,1,0,0,1,0,0,1,0,1,1,1,0,1,1,1,0,0,1,1,0},
    {0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,1,0,1,0,0,0,1,1,1,1,0,0},
    {0,1,1,1,1,0,1,1,0,1,1,1,1,1,0,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,0,1,1,1,1,1,0,1,1,1,1,0,0,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1},
    {0,1,1,1,1,0,1,1,0,1,1,1,1,1,0,1,1,1,1,0,1,1,1,1,1,1,1,0,1,1,1,1},
    {0,1,1,1,1,0,1,1,0,1,0,0,1,0,1,1,1,1,0,0,1,1,1,1,0,1,1,1,0,1,1,1},
    {0,1,1,1,1,0,1,1,0,1,0,0,1,0,0,1,1,1,0,0,1,1,1,1,1,1,1,1,0,1,1,1},
    {0,1,1,1,1,0,1,1,0,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,0,1,0,0,1,0,1,0,0,0,0,1,1,0,0,0,0,1,1,0,1,0,0,1,1,1,1,1,1,1},
    {1,1,1,1,1,0,1,0,1,1,0,1,1,0,1,1,1,0,0,1,1,1,0,1,0,1,1,1,1,1,0,0},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,0,1,1,0,1,1,1,1,1,1,1,1,0,1,1,1,0,0,1,1,1,1,0,1,0,1,1,1,1,1,1},
    {1,1,0,0,0,0,0,0,1,1,0,0,0,0,1,1,1,0,0,1,1,0,1,1,1,0,1,1,1,1,0,0}
};

/*------------------------------------------------------------------------*/

int SubstTable[][2] =
{
    {NBSP,         ASCII_SUBST + ' '},

    {SHY,          ASCII_SUBST + '-'},
    {EnDash,       ASCII_SUBST + '-'},
    {EmDash,       ASCII_SUBST + '-'},

    {CapYo,        CapE},
    {SmallYo,      SmallE},

    {CapGje,       CapG},
    {SmallGje,     SmallG},

    {CapGhe,       CapG},
    {SmallGhe,     SmallG},

    {CapDze,       'S'},
    {SmallDze,     's'},

    {CapKje,       CapK},
    {SmallKje,     SmallK},

    {CapShortU,    CapU},
    {SmallShortU,  SmallU},

    {CapJe,        ASCII_SUBST + 'J'},
    {SmallJe,      ASCII_SUBST + 'j'},

    {CapI_lat,     ASCII_SUBST + 'I'},
    {SmallI_lat,   ASCII_SUBST + 'i'},

    {CapYi,        ASCII_SUBST + 'I'},
    {SmallYi,      ASCII_SUBST + 'i'},

    {Number,         ASCII_SUBST + 'N'},
    {BrokenVertBar,  ASCII_SUBST + '|'},

    {Bullet,       SmallSquare},

    {FScript,      ASCII_SUBST + 'f'},

    {SLeftGuillemet,  DLeftGuillemet},
    {SRightGuillemet, DRightGuillemet},
    {SLeftGuillemet,  ASCII_SUBST + '<'},
    {SRightGuillemet, ASCII_SUBST + '>'},
    {DLeftGuillemet,  ASCII_SUBST + '<'},
    {DRightGuillemet, ASCII_SUBST + '>'},

    {SQuoteBottom, SQuoteOpen},
    {SQuoteBottom, ASCII_SUBST + '\''},
    {SQuoteOpen,   ASCII_SUBST + '\''},
    {SQuoteClose,  ASCII_SUBST + '\''},

    {DQuoteBottom, DQuoteOpen},
    {DQuoteBottom, ASCII_SUBST + '\"'},
    {DQuoteOpen,   ASCII_SUBST + '\"'},
    {DQuoteClose,  ASCII_SUBST + '\"'},

    {0,0}
};

_charset charsets[N_CHARSETS] =
{
    {EN_ALT, CharsetAlternative},
    {EN_KOI8R, CharsetKOI8R},
    {EN_MAC, CharsetMacintosh},
    {EN_ISO, CharsetISO_8859},
    {EN_WINDOWS, CharsetWindows},
};
