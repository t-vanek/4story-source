using System;
using System.Collections.Generic;
using FourStory.Persistence.Game;
using Microsoft.EntityFrameworkCore;

namespace FourStory.Persistence;

public partial class GameDbContext : DbContext
{
    public GameDbContext(DbContextOptions<GameDbContext> options)
        : base(options)
    {
    }

    public virtual DbSet<ACCESSORYMAGICTABLE> ACCESSORYMAGICTABLEs { get; set; }

    public virtual DbSet<OTESTER> OTESTERs { get; set; }

    public virtual DbSet<SKILLLINK> SKILLLINKs { get; set; }

    public virtual DbSet<STATISTICACCOUNT> STATISTICACCOUNTs { get; set; }

    public virtual DbSet<STATISTICTEMP> STATISTICTEMPs { get; set; }

    public virtual DbSet<TACTIVECHARTABLE> TACTIVECHARTABLEs { get; set; }

    public virtual DbSet<TAICHART> TAICHARTs { get; set; }

    public virtual DbSet<TAICMDCHART> TAICMDCHARTs { get; set; }

    public virtual DbSet<TAICONCHART> TAICONCHARTs { get; set; }

    public virtual DbSet<TAIDTABLE> TAIDTABLEs { get; set; }

    public virtual DbSet<TALLCHARVIEW> TALLCHARVIEWs { get; set; }

    public virtual DbSet<TALLITEM_ARCHER1> TALLITEM_ARCHER1s { get; set; }

    public virtual DbSet<TALLITEM_PRIEST1> TALLITEM_PRIEST1s { get; set; }

    public virtual DbSet<TALLITEM_RANGER1> TALLITEM_RANGER1s { get; set; }

    public virtual DbSet<TALLITEM_SORCERER1> TALLITEM_SORCERER1s { get; set; }

    public virtual DbSet<TALLITEM_WARRIOR1> TALLITEM_WARRIOR1s { get; set; }

    public virtual DbSet<TALLITEM_WIZARD1> TALLITEM_WIZARD1s { get; set; }

    public virtual DbSet<TALLSKILL_WIZARD1> TALLSKILL_WIZARD1s { get; set; }

    public virtual DbSet<TARENACHART> TARENACHARTs { get; set; }

    public virtual DbSet<TAUCTIONBIDDER> TAUCTIONBIDDERs { get; set; }

    public virtual DbSet<TAUCTIONINTEREST> TAUCTIONINTERESTs { get; set; }

    public virtual DbSet<TAUCTIONTABLE> TAUCTIONTABLEs { get; set; }

    public virtual DbSet<TBATTLERANKCHART> TBATTLERANKCHARTs { get; set; }

    public virtual DbSet<TBATTLETIMECHART> TBATTLETIMECHARTs { get; set; }

    public virtual DbSet<TBATTLEZONECHART> TBATTLEZONECHARTs { get; set; }

    public virtual DbSet<TBOWBONUSITEMCHART> TBOWBONUSITEMCHARTs { get; set; }

    public virtual DbSet<TBOWITEMCHART> TBOWITEMCHARTs { get; set; }

    public virtual DbSet<TBOWITEMCHART1> TBOWITEMCHART1s { get; set; }

    public virtual DbSet<TBOWSETTINGSCHART> TBOWSETTINGSCHARTs { get; set; }

    public virtual DbSet<TBRPLAYERTABLE> TBRPLAYERTABLEs { get; set; }

    public virtual DbSet<TBRSETTINGSCHART> TBRSETTINGSCHARTs { get; set; }

    public virtual DbSet<TBRSPAWNPOSCHART> TBRSPAWNPOSCHARTs { get; set; }

    public virtual DbSet<TBRSUPPLIESCHART> TBRSUPPLIESCHARTs { get; set; }

    public virtual DbSet<TCABINETITEMTABLE> TCABINETITEMTABLEs { get; set; }

    public virtual DbSet<TCABINETTABLE> TCABINETTABLEs { get; set; }

    public virtual DbSet<TCASTLEAPPLICANTTABLE> TCASTLEAPPLICANTTABLEs { get; set; }

    public virtual DbSet<TCASTLETABLE> TCASTLETABLEs { get; set; }

    public virtual DbSet<TCHANGEDITEM> TCHANGEDITEMs { get; set; }

    public virtual DbSet<TCHANNEL> TCHANNELs { get; set; }

    public virtual DbSet<TCHANNELCHART> TCHANNELCHARTs { get; set; }

    public virtual DbSet<TCHARTABLE> TCHARTABLEs { get; set; }

    public virtual DbSet<TCLASSCHART> TCLASSCHARTs { get; set; }

    public virtual DbSet<TCMGIFTCHART> TCMGIFTCHARTs { get; set; }

    public virtual DbSet<TCMGIFTTABLE> TCMGIFTTABLEs { get; set; }

    public virtual DbSet<TCOMPANIONBONUSCHART> TCOMPANIONBONUSCHARTs { get; set; }

    public virtual DbSet<TCOMPANIONBONUSCHART_EMPTY> TCOMPANIONBONUSCHART_EMPTies { get; set; }

    public virtual DbSet<TCOMPANIONITEMTABLE> TCOMPANIONITEMTABLEs { get; set; }

    public virtual DbSet<TCOMPANIONTABLE> TCOMPANIONTABLEs { get; set; }

    public virtual DbSet<TCUSTOMCLOAKTABLE> TCUSTOMCLOAKTABLEs { get; set; }

    public virtual DbSet<TCUSTOMTIMECHART> TCUSTOMTIMECHARTs { get; set; }

    public virtual DbSet<TDBITEMINDEXTABLE> TDBITEMINDEXTABLEs { get; set; }

    public virtual DbSet<TDESTINATIONCHART> TDESTINATIONCHARTs { get; set; }

    public virtual DbSet<TDUELCHARTABLE> TDUELCHARTABLEs { get; set; }

    public virtual DbSet<TDUELSCORETABLE> TDUELSCORETABLEs { get; set; }

    public virtual DbSet<TEQUIPCREATECHARCHART> TEQUIPCREATECHARCHARTs { get; set; }

    public virtual DbSet<TERASECHARLOG> TERASECHARLOGs { get; set; }

    public virtual DbSet<TERASEITEM> TERASEITEMs { get; set; }

    public virtual DbSet<TEST> TESTs { get; set; }

    public virtual DbSet<TEVENTITEMTABLE> TEVENTITEMTABLEs { get; set; }

    public virtual DbSet<TEVENTQUARTERCHART> TEVENTQUARTERCHARTs { get; set; }

    public virtual DbSet<TEVENTQUARTERGIVETABLE> TEVENTQUARTERGIVETABLEs { get; set; }

    public virtual DbSet<TEXPITEMTABLE> TEXPITEMTABLEs { get; set; }

    public virtual DbSet<TFAKECHARCHART> TFAKECHARCHARTs { get; set; }

    public virtual DbSet<TFAKESHOPCHART> TFAKESHOPCHARTs { get; set; }

    public virtual DbSet<TFORMULACHART> TFORMULACHARTs { get; set; }

    public virtual DbSet<TFRIENDGROUPTABLE> TFRIENDGROUPTABLEs { get; set; }

    public virtual DbSet<TFRIENDTABLE> TFRIENDTABLEs { get; set; }

    public virtual DbSet<TGAMBLECHART> TGAMBLECHARTs { get; set; }

    public virtual DbSet<TGATECHART> TGATECHARTs { get; set; }

    public virtual DbSet<TGEMGRADECHART> TGEMGRADECHARTs { get; set; }

    public virtual DbSet<TGMREWARDCHART> TGMREWARDCHARTs { get; set; }

    public virtual DbSet<TGMREWARDTABLE> TGMREWARDTABLEs { get; set; }

    public virtual DbSet<TGODBALLCHART> TGODBALLCHARTs { get; set; }

    public virtual DbSet<TGODTOWERCHART> TGODTOWERCHARTs { get; set; }

    public virtual DbSet<TGUILDARTICLETABLE> TGUILDARTICLETABLEs { get; set; }

    public virtual DbSet<TGUILDCABINETTABLE> TGUILDCABINETTABLEs { get; set; }

    public virtual DbSet<TGUILDCHART> TGUILDCHARTs { get; set; }

    public virtual DbSet<TGUILDMEMBER> TGUILDMEMBERs { get; set; }

    public virtual DbSet<TGUILDMEMBERSKILLTABLE> TGUILDMEMBERSKILLTABLEs { get; set; }

    public virtual DbSet<TGUILDMEMBERTABLE> TGUILDMEMBERTABLEs { get; set; }

    public virtual DbSet<TGUILDPLAYLOG> TGUILDPLAYLOGs { get; set; }

    public virtual DbSet<TGUILDPVPOINTREWARDTABLE> TGUILDPVPOINTREWARDTABLEs { get; set; }

    public virtual DbSet<TGUILDPVPRECORDTABLE> TGUILDPVPRECORDTABLEs { get; set; }

    public virtual DbSet<TGUILDRELATION> TGUILDRELATIONs { get; set; }

    public virtual DbSet<TGUILDSKILLCHART> TGUILDSKILLCHARTs { get; set; }

    public virtual DbSet<TGUILDSTATSTABLE> TGUILDSTATSTABLEs { get; set; }

    public virtual DbSet<TGUILDTABLE> TGUILDTABLEs { get; set; }

    public virtual DbSet<TGUILDTABLE_copy> TGUILDTABLE_copies { get; set; }

    public virtual DbSet<TGUILDTACTICSTABLE> TGUILDTACTICSTABLEs { get; set; }

    public virtual DbSet<TGUILDTACTICSWANTEDTABLE> TGUILDTACTICSWANTEDTABLEs { get; set; }

    public virtual DbSet<TGUILDVOLUNTEERTABLE> TGUILDVOLUNTEERTABLEs { get; set; }

    public virtual DbSet<TGUILDWANTEDTABLE> TGUILDWANTEDTABLEs { get; set; }

    public virtual DbSet<THELPMESSAGETABLE> THELPMESSAGETABLEs { get; set; }

    public virtual DbSet<THEROTABLE> THEROTABLEs { get; set; }

    public virtual DbSet<THOTKEYTABLE> THOTKEYTABLEs { get; set; }

    public virtual DbSet<TINDUNCHART> TINDUNCHARTs { get; set; }

    public virtual DbSet<TINVENTABLE> TINVENTABLEs { get; set; }

    public virtual DbSet<TINVENTOURNAMENTCHART> TINVENTOURNAMENTCHARTs { get; set; }

    public virtual DbSet<TITEMATTRCHART> TITEMATTRCHARTs { get; set; }

    public virtual DbSet<TITEMCHANGE> TITEMCHANGEs { get; set; }

    public virtual DbSet<TITEMCHART> TITEMCHARTs { get; set; }

    public virtual DbSet<TITEMCHART_bak> TITEMCHART_baks { get; set; }

    public virtual DbSet<TITEMGRADECHART> TITEMGRADECHARTs { get; set; }

    public virtual DbSet<TITEMLEVELCHART> TITEMLEVELCHARTs { get; set; }

    public virtual DbSet<TITEMLOG> TITEMLOGs { get; set; }

    public virtual DbSet<TITEMMAGICCHART> TITEMMAGICCHARTs { get; set; }

    public virtual DbSet<TITEMMAGICLEVELCHART> TITEMMAGICLEVELCHARTs { get; set; }

    public virtual DbSet<TITEMMAGICSKILLCHART> TITEMMAGICSKILLCHARTs { get; set; }

    public virtual DbSet<TITEMSETCHART> TITEMSETCHARTs { get; set; }

    public virtual DbSet<TITEMTABLE> TITEMTABLEs { get; set; }

    public virtual DbSet<TITEMTOURNAMENTCHART> TITEMTOURNAMENTCHARTs { get; set; }

    public virtual DbSet<TITEMUSEDTABLE> TITEMUSEDTABLEs { get; set; }

    public virtual DbSet<TLASTCOMPANIONTABLE> TLASTCOMPANIONTABLEs { get; set; }

    public virtual DbSet<TLASTMONTHPOINTTABLE> TLASTMONTHPOINTTABLEs { get; set; }

    public virtual DbSet<TLASTTOTALPOINTTABLE> TLASTTOTALPOINTTABLEs { get; set; }

    public virtual DbSet<TLEVELCHART> TLEVELCHARTs { get; set; }

    public virtual DbSet<TLOCALOCCUPYTABLE> TLOCALOCCUPYTABLEs { get; set; }

    public virtual DbSet<TLOCALTABLE> TLOCALTABLEs { get; set; }

    public virtual DbSet<TMAPCHART> TMAPCHARTs { get; set; }

    public virtual DbSet<TMAPMONCHART> TMAPMONCHARTs { get; set; }

    public virtual DbSet<TMEDAL> TMEDALs { get; set; }

    public virtual DbSet<TMENTORTABLE> TMENTORTABLEs { get; set; }

    public virtual DbSet<TMISSIONTABLE> TMISSIONTABLEs { get; set; }

    public virtual DbSet<TMONATTRCHART> TMONATTRCHARTs { get; set; }

    public virtual DbSet<TMONITEMCHART> TMONITEMCHARTs { get; set; }

    public virtual DbSet<TMONSPAWNCHART> TMONSPAWNCHARTs { get; set; }

    public virtual DbSet<TMONSTERCHART> TMONSTERCHARTs { get; set; }

    public virtual DbSet<TMONSTERSHOPCHART> TMONSTERSHOPCHARTs { get; set; }

    public virtual DbSet<TMONTHPVPOINTTABLE> TMONTHPVPOINTTABLEs { get; set; }

    public virtual DbSet<TMONTHRANKCHART> TMONTHRANKCHARTs { get; set; }

    public virtual DbSet<TMONTHRANKTABLE> TMONTHRANKTABLEs { get; set; }

    public virtual DbSet<TMOUNTCHART> TMOUNTCHARTs { get; set; }

    public virtual DbSet<TMOUNTITEMTABLE> TMOUNTITEMTABLEs { get; set; }

    public virtual DbSet<TMOUNTTABLE> TMOUNTTABLEs { get; set; }

    public virtual DbSet<TNPCCHART> TNPCCHARTs { get; set; }

    public virtual DbSet<TNPCITEMCHART> TNPCITEMCHARTs { get; set; }

    public virtual DbSet<TOPERATORTABLE> TOPERATORTABLEs { get; set; }

    public virtual DbSet<TPETCHART> TPETCHARTs { get; set; }

    public virtual DbSet<TPETTABLE> TPETTABLEs { get; set; }

    public virtual DbSet<TPLAYTIMETABLE> TPLAYTIMETABLEs { get; set; }

    public virtual DbSet<TPORTALCHART> TPORTALCHARTs { get; set; }

    public virtual DbSet<TPOSTERRORTABLE> TPOSTERRORTABLEs { get; set; }

    public virtual DbSet<TPOSTITEMTABLE> TPOSTITEMTABLEs { get; set; }

    public virtual DbSet<TPOSTTABLE> TPOSTTABLEs { get; set; }

    public virtual DbSet<TPREMIUMSKILLCHART> TPREMIUMSKILLCHARTs { get; set; }

    public virtual DbSet<TPROTECTEDTABLE> TPROTECTEDTABLEs { get; set; }

    public virtual DbSet<TPVPOINTCHART> TPVPOINTCHARTs { get; set; }

    public virtual DbSet<TPVPOINTTABLE> TPVPOINTTABLEs { get; set; }

    public virtual DbSet<TPVPRECENTTABLE> TPVPRECENTTABLEs { get; set; }

    public virtual DbSet<TPVPRECORDTABLE> TPVPRECORDTABLEs { get; set; }

    public virtual DbSet<TQCLASSCHART> TQCLASSCHARTs { get; set; }

    public virtual DbSet<TQCONDITIONCHART> TQCONDITIONCHARTs { get; set; }

    public virtual DbSet<TQREWARDCHART> TQREWARDCHARTs { get; set; }

    public virtual DbSet<TQTITLECHART> TQTITLECHARTs { get; set; }

    public virtual DbSet<TQUESTCHART> TQUESTCHARTs { get; set; }

    public virtual DbSet<TQUESTITEMCHART> TQUESTITEMCHARTs { get; set; }

    public virtual DbSet<TQUESTTABLE> TQUESTTABLEs { get; set; }

    public virtual DbSet<TQUESTTERMCHART> TQUESTTERMCHARTs { get; set; }

    public virtual DbSet<TQUESTTERMTABLE> TQUESTTERMTABLEs { get; set; }

    public virtual DbSet<TRACECHART> TRACECHARTs { get; set; }

    public virtual DbSet<TRANKING> TRANKINGs { get; set; }

    public virtual DbSet<TRECALLMAINTAINTABLE> TRECALLMAINTAINTABLEs { get; set; }

    public virtual DbSet<TRECALLMONTABLE> TRECALLMONTABLEs { get; set; }

    public virtual DbSet<TRESERVEDPOST> TRESERVEDPOSTs { get; set; }

    public virtual DbSet<TRPSGAMECHART> TRPSGAMECHARTs { get; set; }

    public virtual DbSet<TRPSGAMERECORDTABLE> TRPSGAMERECORDTABLEs { get; set; }

    public virtual DbSet<TSAVEDCUSTOMCLOAKTABLE> TSAVEDCUSTOMCLOAKTABLEs { get; set; }

    public virtual DbSet<TSKILLCHART> TSKILLCHARTs { get; set; }

    public virtual DbSet<TSKILLDATum> TSKILLDATAs { get; set; }

    public virtual DbSet<TSKILLLOG> TSKILLLOGs { get; set; }

    public virtual DbSet<TSKILLMAINTAINTABLE> TSKILLMAINTAINTABLEs { get; set; }

    public virtual DbSet<TSKILLPOINTCHART> TSKILLPOINTCHARTs { get; set; }

    public virtual DbSet<TSKILLREWARD> TSKILLREWARDs { get; set; }

    public virtual DbSet<TSKILLREWARDMONEY> TSKILLREWARDMONEYs { get; set; }

    public virtual DbSet<TSKILLTABLE> TSKILLTABLEs { get; set; }

    public virtual DbSet<TSKYGARDENTABLE> TSKYGARDENTABLEs { get; set; }

    public virtual DbSet<TSOULMATETABLE> TSOULMATETABLEs { get; set; }

    public virtual DbSet<TSPAWNPATHCHART> TSPAWNPATHCHARTs { get; set; }

    public virtual DbSet<TSPAWNPOSCHART> TSPAWNPOSCHARTs { get; set; }

    public virtual DbSet<TSPECIALBOXCHART> TSPECIALBOXCHARTs { get; set; }

    public virtual DbSet<TSTARTHOTKEY> TSTARTHOTKEYs { get; set; }

    public virtual DbSet<TSTARTITEMCHART> TSTARTITEMCHARTs { get; set; }

    public virtual DbSet<TSTARTRECALL> TSTARTRECALLs { get; set; }

    public virtual DbSet<TSTARTSKILL> TSTARTSKILLs { get; set; }

    public virtual DbSet<TSVRCHART> TSVRCHARTs { get; set; }

    public virtual DbSet<TSVRMSGCHART> TSVRMSGCHARTs { get; set; }

    public virtual DbSet<TSWITCHCHART> TSWITCHCHARTs { get; set; }

    public virtual DbSet<TTAXTABLE> TTAXTABLEs { get; set; }

    public virtual DbSet<TTEMPCABINETITEMTABLE> TTEMPCABINETITEMTABLEs { get; set; }

    public virtual DbSet<TTEMPCABINETTABLE> TTEMPCABINETTABLEs { get; set; }

    public virtual DbSet<TTEMPEXPITEMTABLE> TTEMPEXPITEMTABLEs { get; set; }

    public virtual DbSet<TTEMPINVENTABLE> TTEMPINVENTABLEs { get; set; }

    public virtual DbSet<TTEMPITEMTABLE> TTEMPITEMTABLEs { get; set; }

    public virtual DbSet<TTEMPITEMUSEDTABLE> TTEMPITEMUSEDTABLEs { get; set; }

    public virtual DbSet<TTEMPSKILLMAINTAINTABLE> TTEMPSKILLMAINTAINTABLEs { get; set; }

    public virtual DbSet<TTEMPSKILLTABLE> TTEMPSKILLTABLEs { get; set; }

    public virtual DbSet<TTITLECHART> TTITLECHARTs { get; set; }

    public virtual DbSet<TTITLETABLE> TTITLETABLEs { get; set; }

    public virtual DbSet<TTNMTEVENTREWARDTABLE> TTNMTEVENTREWARDTABLEs { get; set; }

    public virtual DbSet<TTNMTEVENTSCHEDULETABLE> TTNMTEVENTSCHEDULETABLEs { get; set; }

    public virtual DbSet<TTNMTEVENTTABLE> TTNMTEVENTTABLEs { get; set; }

    public virtual DbSet<TTNMTEVENTTIMETABLE> TTNMTEVENTTIMETABLEs { get; set; }

    public virtual DbSet<TTOURNAMENTCHART> TTOURNAMENTCHARTs { get; set; }

    public virtual DbSet<TTOURNAMENTPLAYERTABLE> TTOURNAMENTPLAYERTABLEs { get; set; }

    public virtual DbSet<TTOURNAMENTREWARDCHART> TTOURNAMENTREWARDCHARTs { get; set; }

    public virtual DbSet<TTOURNAMENTREWARDCHART_weap> TTOURNAMENTREWARDCHART_weaps { get; set; }

    public virtual DbSet<TTOURNAMENTSCHEDULECHART> TTOURNAMENTSCHEDULECHARTs { get; set; }

    public virtual DbSet<TTOURNAMENTSTATUSTABLE> TTOURNAMENTSTATUSTABLEs { get; set; }

    public virtual DbSet<TUNIFYPET> TUNIFYPETs { get; set; }

    public virtual DbSet<TUNITCHART> TUNITCHARTs { get; set; }

    public virtual DbSet<TVIEW_CASHCATEGORYCHART> TVIEW_CASHCATEGORYCHARTs { get; set; }

    public virtual DbSet<TVIEW_CASHGAMBLECHART> TVIEW_CASHGAMBLECHARTs { get; set; }

    public virtual DbSet<TVIEW_CASHITEMCABINET> TVIEW_CASHITEMCABINETs { get; set; }

    public virtual DbSet<TVIEW_CASHSHOPITEMCHART> TVIEW_CASHSHOPITEMCHARTs { get; set; }

    public virtual DbSet<TVIEW_CHARTABLE> TVIEW_CHARTABLEs { get; set; }

    public virtual DbSet<TVIEW_DURINGITEMTABLE> TVIEW_DURINGITEMTABLEs { get; set; }

    public virtual DbSet<TVIEW_GUILDTACTICSTABLE> TVIEW_GUILDTACTICSTABLEs { get; set; }

    public virtual DbSet<TVIEW_GUILDVOLUNTEERTABLE> TVIEW_GUILDVOLUNTEERTABLEs { get; set; }

    public virtual DbSet<TVIEW_MENTORMEMBER> TVIEW_MENTORMEMBERs { get; set; }

    public virtual DbSet<TVIEW_MONTHRANK> TVIEW_MONTHRANKs { get; set; }

    public virtual DbSet<TVIEW_SKILLPOINT> TVIEW_SKILLPOINTs { get; set; }

    public virtual DbSet<TVIEW_SOULMATE> TVIEW_SOULMATEs { get; set; }

    public virtual DbSet<TVIEW_TOURNAMENTPLAYER> TVIEW_TOURNAMENTPLAYERs { get; set; }

    public virtual DbSet<charkilling_log> charkilling_logs { get; set; }

    public virtual DbSet<dtproperty> dtproperties { get; set; }

    public virtual DbSet<tguildview> tguildviews { get; set; }

    public virtual DbSet<tviewguildplayer> tviewguildplayers { get; set; }

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.UseCollation("Latin1_General_CI_AS_KS");

        modelBuilder.Entity<ACCESSORYMAGICTABLE>(entity =>
        {
            entity.HasKey(e => e.ID).HasName("PK__ACCESSOR__3214EC27467F8076");

            entity.ToTable("ACCESSORYMAGICTABLE");

            entity.Property(e => e.ID).ValueGeneratedNever();
        });

        modelBuilder.Entity<OTESTER>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("OTESTER");
        });

        modelBuilder.Entity<SKILLLINK>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("SKILLLINK");
        });

        modelBuilder.Entity<STATISTICACCOUNT>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("STATISTICACCOUNT");
        });

        modelBuilder.Entity<STATISTICTEMP>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("STATISTICTEMP");
        });

        modelBuilder.Entity<TACTIVECHARTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TACTIVEC__B92C9D036CE4AC70")
                .HasFillFactor(80);

            entity.ToTable("TACTIVECHARTABLE");

            entity.HasIndex(e => e.dateEnter, "IX_TACTIVECHARTABLE").HasFillFactor(80);

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
            entity.Property(e => e.dateEnter).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TAICHART>(entity =>
        {
            entity.HasKey(e => new { e.bAIType, e.dwCmdID, e.bTriggerType, e.dwTriggerID })
                .HasName("PK__TAICHART__8863207C365E7E8D")
                .HasFillFactor(80);

            entity.ToTable("TAICHART");

            entity.HasIndex(e => e.bAIType, "IX_TAICHART").HasFillFactor(80);
        });

        modelBuilder.Entity<TAICMDCHART>(entity =>
        {
            entity.HasKey(e => e.dwCmdID)
                .HasName("PK__TAICMDCH__E1DB92C4393AEB38")
                .HasFillFactor(80);

            entity.ToTable("TAICMDCHART");

            entity.Property(e => e.dwCmdID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TAICONCHART>(entity =>
        {
            entity.HasKey(e => new { e.dwCmdID, e.bConditionType })
                .HasName("PK__TAICONCH__2433FB783C1757E3")
                .HasFillFactor(80);

            entity.ToTable("TAICONCHART");

            entity.HasIndex(e => e.dwCmdID, "IX_TAICONCHART").HasFillFactor(80);
        });

        modelBuilder.Entity<TAIDTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TAIDTABL__B92C9D037C26F000")
                .HasFillFactor(80);

            entity.ToTable("TAIDTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
            entity.Property(e => e.dDate).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TALLCHARVIEW>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLCHARVIEW");

            entity.Property(e => e.dCreateDate).HasColumnType("datetime");
            entity.Property(e => e.dDeleteDate).HasColumnType("datetime");
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TALLITEM_ARCHER1>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLITEM_ARCHER1");

            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TALLITEM_PRIEST1>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLITEM_PRIEST1");

            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TALLITEM_RANGER1>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLITEM_RANGER1");

            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TALLITEM_SORCERER1>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLITEM_SORCERER1");

            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TALLITEM_WARRIOR1>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLITEM_WARRIOR1");

            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TALLITEM_WIZARD1>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLITEM_WIZARD1");

            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TALLSKILL_WIZARD1>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TALLSKILL_WIZARD1");
        });

        modelBuilder.Entity<TARENACHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TARENACH__30FE57433EF3C48E")
                .HasFillFactor(80);

            entity.ToTable("TARENACHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TAUCTIONBIDDER>(entity =>
        {
            entity.HasKey(e => new { e.dwAuctionID, e.dwCharID })
                .HasName("PK__TAUCTION__8AA1FCC403C811C8")
                .HasFillFactor(80);

            entity.ToTable("TAUCTIONBIDDER");

            entity.Property(e => e.DateBid).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TAUCTIONINTEREST>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.dwAuctionID })
                .HasName("PK__TAUCTION__E33FAE520798A2AC")
                .HasFillFactor(80);

            entity.ToTable("TAUCTIONINTEREST");

            entity.HasIndex(e => new { e.dwCharID, e.dwAuctionID }, "IX_TAUCTIONINTEREST").HasFillFactor(80);
        });

        modelBuilder.Entity<TAUCTIONTABLE>(entity =>
        {
            entity.HasKey(e => e.dwAuctionID)
                .HasName("PK__TAUCTION__A133351441D03139")
                .HasFillFactor(80);

            entity.ToTable("TAUCTIONTABLE");

            entity.Property(e => e.DateEnd).HasColumnType("smalldatetime");
            entity.Property(e => e.DateStart).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TBATTLERANKCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBATTLERANKCHART");
        });

        modelBuilder.Entity<TBATTLETIMECHART>(entity =>
        {
            entity.HasKey(e => e.bType)
                .HasName("PK__TBATTLET__833405D22CC005BA")
                .HasFillFactor(80);

            entity.ToTable("TBATTLETIMECHART");
        });

        modelBuilder.Entity<TBATTLEZONECHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TBATTLEZ__30FE57433278DF10")
                .HasFillFactor(80);

            entity.ToTable("TBATTLEZONECHART");

            entity.HasIndex(e => e.wCastle, "IX_TBATTLEZONECHART_CASTLE").HasFillFactor(80);

            entity.Property(e => e.wID).ValueGeneratedNever();
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TBOWBONUSITEMCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBOWBONUSITEMCHART");
        });

        modelBuilder.Entity<TBOWITEMCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBOWITEMCHART");
        });

        modelBuilder.Entity<TBOWITEMCHART1>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBOWITEMCHART1");
        });

        modelBuilder.Entity<TBOWSETTINGSCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBOWSETTINGSCHART");
        });

        modelBuilder.Entity<TBRPLAYERTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBRPLAYERTABLE");
        });

        modelBuilder.Entity<TBRSETTINGSCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBRSETTINGSCHART");
        });

        modelBuilder.Entity<TBRSPAWNPOSCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBRSPAWNPOSCHART");
        });

        modelBuilder.Entity<TBRSUPPLIESCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TBRSUPPLIESCHART");
        });

        modelBuilder.Entity<TCABINETITEMTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bCabinetID, e.dwStItemID })
                .HasName("PK__TCABINET__A2C2BB4D25290593")
                .HasFillFactor(80);

            entity.ToTable("TCABINETITEMTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TCABINETITEMTABLE_CHAR").HasFillFactor(80);

            entity.HasIndex(e => e.dwCharID, "IX_TCABINETTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TCABINETTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bCabinetID })
                .HasName("PK__TCABINET__83E1E3EF29EDBAB0")
                .HasFillFactor(80);

            entity.ToTable("TCABINETTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TCABINETTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TCASTLEAPPLICANTTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TCASTLEA__B92C9D032DBE4B94")
                .HasFillFactor(80);

            entity.ToTable("TCASTLEAPPLICANTTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TCASTLETABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCASTLETABLE");

            entity.HasIndex(e => e.wCastle, "IX_TCASTLETABLE").HasFillFactor(80);

            entity.Property(e => e.dateHero)
                .HasDefaultValueSql("((0))")
                .HasColumnType("smalldatetime");
            entity.Property(e => e.dateWarTime).HasColumnType("smalldatetime");
            entity.Property(e => e.szHero)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TCHANGEDITEM>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCHANGEDITEM");
        });

        modelBuilder.Entity<TCHANNEL>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TCHANNEL");
        });

        modelBuilder.Entity<TCHANNELCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCHANNELCHART");
        });

        modelBuilder.Entity<TCHARTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TCHARTAB__B92C9D0367D5DE90")
                .HasFillFactor(80);

            entity.ToTable("TCHARTABLE");

            entity.HasIndex(e => new { e.dwCharID, e.bDelete }, "IX_TCHARTABLE_PW_CHARID_DELETE").HasFillFactor(80);

            entity.HasIndex(e => e.szNAME, "IX_TCHARTABLE_PW_NAME").HasFillFactor(80);

            entity.HasIndex(e => e.dwRankPoint, "IX_TCHARTABLE_PW_RANKPOINT").HasFillFactor(80);

            entity.HasIndex(e => e.dwUserID, "IX_TCHARTABLE_PW_USER").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwUserID, e.bCountry, e.bDelete }, "IX_TCHARTABLE_PW_USERID_COUNTRY_DELETE").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwUserID, e.bDelete }, "IX_TCHARTABLE_PW_USERID_DELETE").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwUserID, e.bSlot, e.bDelete }, "IX_TCHARTABLE_PW_USERID_SLOT_DELETE").HasFillFactor(80);

            entity.Property(e => e.bStartAct).HasDefaultValue((byte)1);
            entity.Property(e => e.dCreateDate)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("smalldatetime");
            entity.Property(e => e.dDeleteDate).HasColumnType("smalldatetime");
            entity.Property(e => e.dLogoutDate)
                .HasDefaultValueSql("((0))")
                .HasColumnType("smalldatetime");
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TCLASSCHART>(entity =>
        {
            entity.HasKey(e => e.bClassID)
                .HasName("PK__TCLASSCH__87A39E104D41E3E5")
                .HasFillFactor(80);

            entity.ToTable("TCLASSCHART");
        });

        modelBuilder.Entity<TCMGIFTCHART>(entity =>
        {
            entity.HasKey(e => e.wGiftID)
                .HasName("PK__TCMGIFTC__D3C98F815E6182EF")
                .HasFillFactor(80);

            entity.ToTable("TCMGIFTCHART");

            entity.Property(e => e.szMsg)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szTitle)
                .HasMaxLength(256)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TCMGIFTTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCMGIFTTABLE");

            entity.HasIndex(e => new { e.dwCharID, e.wGiftID }, "IX_TGIFTTABLE_CHARGIFTID").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwUserID, e.wGiftID }, "IX_TGIFTTABLE_USERGIFTID").HasFillFactor(80);

            entity.Property(e => e.tTakeDate).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TCOMPANIONBONUSCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCOMPANIONBONUSCHART");
        });

        modelBuilder.Entity<TCOMPANIONBONUSCHART_EMPTY>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCOMPANIONBONUSCHART_EMPTY");
        });

        modelBuilder.Entity<TCOMPANIONITEMTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCOMPANIONITEMTABLE");

            entity.Property(e => e.dFirstEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.dSecondEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TCOMPANIONTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCOMPANIONTABLE");

            entity.Property(e => e.strName)
                .HasMaxLength(55)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TCUSTOMCLOAKTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCUSTOMCLOAKTABLE");

            entity.Property(e => e.dDateCreate).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TCUSTOMTIMECHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCUSTOMTIMECHART");
        });

        modelBuilder.Entity<TDBITEMINDEXTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TDBITEMINDEXTABLE");

            entity.Property(e => e.dlID).HasDefaultValue(1L);
        });

        modelBuilder.Entity<TDESTINATIONCHART>(entity =>
        {
            entity.HasKey(e => new { e.wPortalID, e.wDestID })
                .HasName("PK__TDESTINA__E88A603822977D02")
                .HasFillFactor(80);

            entity.ToTable("TDESTINATIONCHART");

            entity.HasIndex(e => e.wPortalID, "IX_TDESTINATIONCHART_PORTAL").HasFillFactor(80);
        });

        modelBuilder.Entity<TDUELCHARTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TDUELCHARTABLE");

            entity.HasIndex(e => new { e.dwCharID, e.dTime }, "IX_TDUELCHARTABLE").HasFillFactor(80);

            entity.Property(e => e.dTime).HasColumnType("datetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TDUELSCORETABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TDUELSCO__B92C9D037080332A")
                .HasFillFactor(80);

            entity.ToTable("TDUELSCORETABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TEQUIPCREATECHARCHART>(entity =>
        {
            entity.HasKey(e => new { e.bCountry, e.bClass, e.bSex })
                .HasName("PK__TEQUIPCR__4F3A3EC952FABD3B")
                .HasFillFactor(80);

            entity.ToTable("TEQUIPCREATECHARCHART");
        });

        modelBuilder.Entity<TERASECHARLOG>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TERASECHARLOG");

            entity.HasIndex(e => e.dwCharID, "IX_TERASECHARLOG").HasFillFactor(80);

            entity.Property(e => e.dEraseDate)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TERASEITEM>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TERASEITEM");
        });

        modelBuilder.Entity<TEST>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TEST");

            entity.Property(e => e.DATA)
                .HasMaxLength(100)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TEVENTITEMTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TEVENTIT__B92C9D037BF1E5D6")
                .HasFillFactor(80);

            entity.ToTable("TEVENTITEMTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TEVENTQUARTERCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TEVENTQU__30FE57437FC276BA")
                .HasFillFactor(80);

            entity.ToTable("TEVENTQUARTERCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
            entity.Property(e => e.szAnnounce)
                .HasMaxLength(1024)
                .IsUnicode(false)
                .HasDefaultValue("");
            entity.Property(e => e.szMessage)
                .HasMaxLength(500)
                .IsUnicode(false);
            entity.Property(e => e.szPresent)
                .HasMaxLength(50)
                .IsUnicode(false)
                .HasDefaultValue("");
            entity.Property(e => e.szTitle)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TEVENTQUARTERGIVETABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TEVENTQUARTERGIVETABLE");

            entity.Property(e => e.dGiveDate)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("smalldatetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TEXPITEMTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TEXPITEMTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TFAKECHARCHART>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TFAKECHA__B92C9D030857BCBB")
                .HasFillFactor(80);

            entity.ToTable("TFAKECHARCHART");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TFAKESHOPCHART>(entity =>
        {
            entity.HasKey(e => new { e.wIndex, e.bCountry })
                .HasName("PK__TFAKESHO__FCD3804E0D1C71D8")
                .HasFillFactor(80);

            entity.ToTable("TFAKESHOPCHART");
        });

        modelBuilder.Entity<TFORMULACHART>(entity =>
        {
            entity.HasKey(e => e.bID)
                .HasName("PK__TFORMULA__DE99891F69E90F8B")
                .HasFillFactor(80);

            entity.ToTable("TFORMULACHART");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TFRIENDGROUPTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bGroup })
                .HasName("PK__TFRIENDG__8C5653E214BD93A0")
                .HasFillFactor(80);

            entity.ToTable("TFRIENDGROUPTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TFRIENDGROUPTABLE_CHAR").HasFillFactor(80);

            entity.Property(e => e.szName)
                .HasMaxLength(20)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TFRIENDTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.dwFriendID })
                .HasName("PK__TFRIENDT__1F7F75C3188E2484")
                .HasFillFactor(80);

            entity.ToTable("TFRIENDTABLE");

            entity.HasIndex(e => e.dwFriendID, "IX_TFRIENDTABLE").HasFillFactor(80);

            entity.HasIndex(e => e.dwCharID, "IX_TFRIENDTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TGAMBLECHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGAMBLECHART");
        });

        modelBuilder.Entity<TGATECHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGATECHART");
        });

        modelBuilder.Entity<TGEMGRADECHART>(entity =>
        {
            entity.HasKey(e => e.bGem)
                .HasName("PK__TGEMGRAD__04329D602A389ECA")
                .HasFillFactor(80);

            entity.ToTable("TGEMGRADECHART");
        });

        modelBuilder.Entity<TGMREWARDCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGMREWARDCHART");

            entity.HasIndex(e => e.bLevel, "IX_TGMREWARDCHART").HasFillFactor(80);
        });

        modelBuilder.Entity<TGMREWARDTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.bLevel })
                .HasName("PK__TGMREWAR__C5ACC33B202F464C")
                .HasFillFactor(80);

            entity.ToTable("TGMREWARDTABLE");

            entity.Property(e => e.dDate).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TGODBALLCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TGODBALL__30FE574323FFD730")
                .HasFillFactor(80);

            entity.ToTable("TGODBALLCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TGODTOWERCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TGODTOWE__30FE574327D06814")
                .HasFillFactor(80);

            entity.ToTable("TGODTOWERCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TGUILDARTICLETABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwGuildID, e.dwID })
                .HasName("PK__TGUILDAR__B9CA43652BA0F8F8")
                .HasFillFactor(80);

            entity.ToTable("TGUILDARTICLETABLE");

            entity.HasIndex(e => e.dwGuildID, "IX_TGUILDARTICLETABLE_GUILDID").HasFillFactor(80);

            entity.Property(e => e.szArticle)
                .HasMaxLength(2048)
                .IsUnicode(false);
            entity.Property(e => e.szTitle)
                .HasMaxLength(256)
                .IsUnicode(false);
            entity.Property(e => e.szWritter)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TGUILDCABINETTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwGuildID, e.dwItemID })
                .HasName("PK__TGUILDCA__9288588D2F7189DC")
                .HasFillFactor(80);

            entity.ToTable("TGUILDCABINETTABLE");

            entity.HasIndex(e => e.dwGuildID, "IX_TGUILDCABINETTABLE_GUILDID").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwGuildID, e.wItemID, e.bCount }, "IX_TGUILDCABINETTABLE_GUILDID_WITEMID_COUNT").HasFillFactor(80);
        });

        modelBuilder.Entity<TGUILDCHART>(entity =>
        {
            entity.HasKey(e => e.bLevel)
                .HasName("PK__TGUILDCH__A29A5ECA34363EF9")
                .HasFillFactor(80);

            entity.ToTable("TGUILDCHART");
        });

        modelBuilder.Entity<TGUILDMEMBER>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TGUILDMEMBER");

            entity.Property(e => e.dLogoutDate).HasColumnType("smalldatetime");
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TGUILDMEMBERSKILLTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGUILDMEMBERSKILLTABLE");

            entity.Property(e => e.tEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TGUILDMEMBERTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TGUILDME__B92C9D033806CFDD")
                .HasFillFactor(80);

            entity.ToTable("TGUILDMEMBERTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TGUILDPLAYLOG>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGUILDPLAYLOG");

            entity.HasIndex(e => e.dwGuildID, "IX_TGUILDPLAYLOG").HasFillFactor(80);
        });

        modelBuilder.Entity<TGUILDPVPOINTREWARDTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGUILDPVPOINTREWARDTABLE");

            entity.HasIndex(e => e.dwGuildID, "IX_TGUILDPVPOINTREWARDTABLE").HasFillFactor(80);

            entity.Property(e => e.dlDate).HasColumnType("smalldatetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TGUILDPVPRECORDTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGUILDPVPRECORDTABLE");

            entity.HasIndex(e => new { e.dwGuildID, e.dwCharID, e.dwDate }, "IX_TGUILDPVPRECORDTABLE").HasFillFactor(80);
        });

        modelBuilder.Entity<TGUILDRELATION>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGUILDRELATION");
        });

        modelBuilder.Entity<TGUILDSKILLCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGUILDSKILLCHART");
        });

        modelBuilder.Entity<TGUILDSTATSTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TGUILDSTATSTABLE");
        });

        modelBuilder.Entity<TGUILDTABLE>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TGUILDTA__2F75250268341ABF")
                .HasFillFactor(80);

            entity.ToTable("TGUILDTABLE");

            entity.HasIndex(e => e.szName, "IX_TGUILDTABLE_NAME").HasFillFactor(80);

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.timeEstablish).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TGUILDTABLE_copy>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TGUILDTA__2F75250287222F78")
                .HasFillFactor(80);

            entity.ToTable("TGUILDTABLE_copy");

            entity.HasIndex(e => e.szName, "IX_TGUILDTABLE_NAME").HasFillFactor(80);

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.timeEstablish).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TGUILDTACTICSTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.dwGuildID })
                .HasName("PK__TGUILDTA__F19F4C1052BAC619")
                .HasFillFactor(80);

            entity.ToTable("TGUILDTACTICSTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TGUILDTACTICSTABLE_CHAR").HasFillFactor(80);

            entity.HasIndex(e => e.dwGuildID, "IX_TGUILDTACTICSTABLE_GUILDID").HasFillFactor(80);

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TGUILDTACTICSWANTEDTABLE>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TGUILDTA__2F752502577F7B36")
                .HasFillFactor(80);

            entity.ToTable("TGUILDTACTICSWANTEDTABLE");

            entity.HasIndex(e => e.dwGuildID, "IX_TGUILDTACTICSWANTEDTABLE").HasFillFactor(80);

            entity.Property(e => e.dwID).ValueGeneratedNever();
            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.szText)
                .HasMaxLength(2048)
                .IsUnicode(false);
            entity.Property(e => e.szTitle)
                .HasMaxLength(256)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TGUILDVOLUNTEERTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bType })
                .HasName("PK__TGUILDVO__811FDD5E5B500C1A")
                .HasFillFactor(80);

            entity.ToTable("TGUILDVOLUNTEERTABLE");
        });

        modelBuilder.Entity<TGUILDWANTEDTABLE>(entity =>
        {
            entity.HasKey(e => e.dwGuildID)
                .HasName("PK__TGUILDWA__8B3D11355F209CFE")
                .HasFillFactor(80);

            entity.ToTable("TGUILDWANTEDTABLE");

            entity.Property(e => e.dwGuildID).ValueGeneratedNever();
            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.szText)
                .HasMaxLength(2048)
                .IsUnicode(false);
            entity.Property(e => e.szTitle)
                .HasMaxLength(256)
                .IsUnicode(false);
        });

        modelBuilder.Entity<THELPMESSAGETABLE>(entity =>
        {
            entity.HasKey(e => e.bID)
                .HasName("PK__THELPMES__DE99891F26680DE6")
                .HasFillFactor(80);

            entity.ToTable("THELPMESSAGETABLE");

            entity.Property(e => e.dEnd).HasColumnType("smalldatetime");
            entity.Property(e => e.dStart).HasColumnType("smalldatetime");
            entity.Property(e => e.szMessage)
                .HasMaxLength(2048)
                .IsUnicode(false);
        });

        modelBuilder.Entity<THEROTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("THEROTABLE");

            entity.Property(e => e.bRace)
                .HasMaxLength(10)
                .IsUnicode(false)
                .IsFixedLength();
            entity.Property(e => e.szGuild)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szSay)
                .HasMaxLength(256)
                .IsUnicode(false)
                .HasDefaultValue("");
        });

        modelBuilder.Entity<THOTKEYTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("THOTKEYTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_THOTKEYTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TINDUNCHART>(entity =>
        {
            entity.HasKey(e => e.wMapID).HasName("PK__TINDUNCH__4713A8584F6A2379");

            entity.ToTable("TINDUNCHART");

            entity.Property(e => e.wMapID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TINVENTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bInvenID })
                .HasName("PK__TINVENTA__688A92BB6A924FAA")
                .HasFillFactor(80);

            entity.ToTable("TINVENTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TINVENTABLE").HasFillFactor(80);

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TINVENTOURNAMENTCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TINVENTOURNAMENTCHART");
        });

        modelBuilder.Entity<TITEMATTRCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TITEMATT__30FE57432CAAF721")
                .HasFillFactor(80);

            entity.ToTable("TITEMATTRCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TITEMCHANGE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TITEMCHANGE");

            entity.HasIndex(e => e.wItemID, "IX_TITEMCHANGE").HasFillFactor(80);
        });

        modelBuilder.Entity<TITEMCHART>(entity =>
        {
            entity.HasKey(e => e.wItemID)
                .HasName("PK__TITEMCHA__7BC077A3448280B2")
                .HasFillFactor(80);

            entity.ToTable("TITEMCHART");

            entity.Property(e => e.wItemID).ValueGeneratedNever();
            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TITEMCHART_bak>(entity =>
        {
            entity.HasKey(e => e.wItemID)
                .HasName("PK__TITEMCHA__7BC077A32E092FAE")
                .HasFillFactor(80);

            entity.ToTable("TITEMCHART_bak");

            entity.Property(e => e.wItemID).ValueGeneratedNever();
            entity.Property(e => e.szNAME).HasMaxLength(100);
        });

        modelBuilder.Entity<TITEMGRADECHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TITEMGRADECHART");
        });

        modelBuilder.Entity<TITEMLEVELCHART>(entity =>
        {
            entity.HasKey(e => e.bGrade)
                .HasName("PK__TITEMLEV__5A20AA5E2A18A339")
                .HasFillFactor(80);

            entity.ToTable("TITEMLEVELCHART");
        });

        modelBuilder.Entity<TITEMLOG>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TITEMLOG");

            entity.HasIndex(e => e.bInsType, "IX_TITEMLOG").HasFillFactor(80);

            entity.Property(e => e.szTargetName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.timeInsert).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TITEMMAGICCHART>(entity =>
        {
            entity.HasKey(e => e.bMagic)
                .HasName("PK__TITEMMAG__1C49BB672EDD5856")
                .HasFillFactor(80);

            entity.ToTable("TITEMMAGICCHART");
        });

        modelBuilder.Entity<TITEMMAGICLEVELCHART>(entity =>
        {
            entity.HasKey(e => e.dwSection)
                .HasName("PK__TITEMMAG__A2E8D934367E7A1E")
                .HasFillFactor(80);

            entity.ToTable("TITEMMAGICLEVELCHART");

            entity.Property(e => e.dwSection).ValueGeneratedNever();
        });

        modelBuilder.Entity<TITEMMAGICSKILLCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TITEMMAGICSKILLCHART");
        });

        modelBuilder.Entity<TITEMSETCHART>(entity =>
        {
            entity.HasKey(e => new { e.wBaseID, e.wSetID })
                .HasName("PK__TITEMSET__995A1B113B432F3B")
                .HasFillFactor(80);

            entity.ToTable("TITEMSETCHART");
        });

        modelBuilder.Entity<TITEMTABLE>(entity =>
        {
            entity.HasKey(e => e.dlID)
                .HasName("PK__TITEMTAB__DA9B90A62D7432D1")
                .HasFillFactor(80);

            entity.ToTable("TITEMTABLE");

            entity.HasIndex(e => new { e.dwOwnerID, e.bOwnerType, e.bStorageType, e.dwStorageID }, "IX_TITEMTABLE").HasFillFactor(80);

            entity.Property(e => e.dlID).ValueGeneratedNever();
            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TITEMTOURNAMENTCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TITEMTOURNAMENTCHART");
        });

        modelBuilder.Entity<TITEMUSEDTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.wDelayGroupID })
                .HasName("PK__TITEMUSE__A4A3CF42583E9545")
                .HasFillFactor(80);

            entity.ToTable("TITEMUSEDTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TITEMUSEDTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TLASTCOMPANIONTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TLASTCOMPANIONTABLE");
        });

        modelBuilder.Entity<TLASTMONTHPOINTTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID).HasFillFactor(80);

            entity.ToTable("TLASTMONTHPOINTTABLE", "tgame");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
            entity.Property(e => e.dwRank).ValueGeneratedOnAdd();
        });

        modelBuilder.Entity<TLASTTOTALPOINTTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID).HasFillFactor(80);

            entity.ToTable("TLASTTOTALPOINTTABLE", "tgame");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
            entity.Property(e => e.dwRank).ValueGeneratedOnAdd();
        });

        modelBuilder.Entity<TLEVELCHART>(entity =>
        {
            entity.HasKey(e => e.bLevel)
                .HasName("PK__TLEVELCH__A29A5ECA5C0F2629")
                .HasFillFactor(80);

            entity.ToTable("TLEVELCHART");
        });

        modelBuilder.Entity<TLOCALOCCUPYTABLE>(entity =>
        {
            entity.HasKey(e => new { e.wLocalID, e.bDay })
                .HasName("PK__TLOCALOC__DEA88FA869692147")
                .HasFillFactor(80);

            entity.ToTable("TLOCALOCCUPYTABLE");
        });

        modelBuilder.Entity<TLOCALTABLE>(entity =>
        {
            entity.HasKey(e => e.wLocalID)
                .HasName("PK__TLOCALTA__2ECCE3826D39B22B")
                .HasFillFactor(80);

            entity.ToTable("TLOCALTABLE");

            entity.Property(e => e.wLocalID).ValueGeneratedNever();
            entity.Property(e => e.dateDefend).HasColumnType("smalldatetime");
            entity.Property(e => e.dateHero)
                .HasDefaultValueSql("((0))")
                .HasColumnType("smalldatetime");
            entity.Property(e => e.dateOccupy).HasColumnType("smalldatetime");
            entity.Property(e => e.szHero)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TMAPCHART>(entity =>
        {
            entity.HasKey(e => new { e.bGroupID, e.wMapID, e.bServerID })
                .HasName("PK__TMAPCHAR__965C2E392B66A5E0")
                .HasFillFactor(80);

            entity.ToTable("TMAPCHART");
        });

        modelBuilder.Entity<TMAPMONCHART>(entity =>
        {
            entity.HasKey(e => new { e.wSpawnID, e.wMonID, e.bEssential })
                .HasName("PK__TMAPMONC__52A4AA58469FD34E")
                .HasFillFactor(80);

            entity.ToTable("TMAPMONCHART");

            entity.HasIndex(e => e.wSpawnID, "IX_TMAPMONCHART").HasFillFactor(80);
        });

        modelBuilder.Entity<TMEDAL>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TMEDALS");
        });

        modelBuilder.Entity<TMENTORTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TMENTORT__B92C9D03799F8910")
                .HasFillFactor(80);

            entity.ToTable("TMENTORTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TMISSIONTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TMISSIONTABLE");
        });

        modelBuilder.Entity<TMONATTRCHART>(entity =>
        {
            entity.HasKey(e => new { e.wID, e.bLevel })
                .HasName("PK__TMONATTR__8AD7F2AF2089CCEA")
                .HasFillFactor(80);

            entity.ToTable("TMONATTRCHART");
        });

        modelBuilder.Entity<TMONITEMCHART>(entity =>
        {
            entity.HasKey(e => new { e.wMonID, e.wItemID, e.bChartType })
                .HasName("PK__TMONITEM__C7D247B055E216DE")
                .HasFillFactor(80);

            entity.ToTable("TMONITEMCHART");

            entity.HasIndex(e => e.wMonID, "IX_TMONITEMCHART_MON").HasFillFactor(80);

            entity.Property(e => e.wWeight).HasDefaultValue((short)1);
        });

        modelBuilder.Entity<TMONSPAWNCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TMONSPAW__30FE57435E775CDF")
                .HasFillFactor(80);

            entity.ToTable("TMONSPAWNCHART");

            entity.HasIndex(e => e.wPartyID, "IX_TMONSPAWNCHART_PARTY").HasFillFactor(80);

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TMONSTERCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TMONSTER__30FE574344E3BAB5")
                .HasFillFactor(80);

            entity.ToTable("TMONSTERCHART");

            entity.HasIndex(e => e.szName, "IX_TMONSTERCHART_NAME_copy1").HasFillFactor(80);

            entity.Property(e => e.wID).ValueGeneratedNever();
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .HasDefaultValueSql("((0))");
            entity.Property(e => e.szName2)
                .HasMaxLength(50)
                .IsUnicode(false)
                .HasDefaultValue("");
        });

        modelBuilder.Entity<TMONSTERSHOPCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TMONSTER__30FE57436CBA8F3E")
                .HasFillFactor(80);

            entity.ToTable("TMONSTERSHOPCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TMONTHPVPOINTTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bCountry })
                .HasName("PK__TMONTHPV__9661723321AD7A6A")
                .HasFillFactor(80);

            entity.ToTable("TMONTHPVPOINTTABLE");

            entity.Property(e => e.szSay)
                .HasMaxLength(256)
                .IsUnicode(false)
                .HasDefaultValue("");
        });

        modelBuilder.Entity<TMONTHRANKCHART>(entity =>
        {
            entity.HasKey(e => e.bRank)
                .HasName("PK__TMONTHRA__71238B2B26722F87")
                .HasFillFactor(80);

            entity.ToTable("TMONTHRANKCHART");

            entity.Property(e => e.bChartType1).HasDefaultValue((byte)1);
            entity.Property(e => e.bChartType2).HasDefaultValue((byte)1);
            entity.Property(e => e.bChartType3).HasDefaultValue((byte)1);
            entity.Property(e => e.bChartType4).HasDefaultValue((byte)1);
        });

        modelBuilder.Entity<TMONTHRANKTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TMONTHRANKTABLE");

            entity.Property(e => e.szGuild)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szSay)
                .HasMaxLength(256)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TMOUNTCHART>(entity =>
        {
            entity.HasKey(e => e.wMountID)
                .HasName("PK__TMOUNTCH__1AA6E4B586727322")
                .HasFillFactor(80);

            entity.ToTable("TMOUNTCHART");

            entity.Property(e => e.wMountID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TMOUNTITEMTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.wItemID })
                .HasName("PK__TMOUNTIT__583961AD9F0C7AC8")
                .HasFillFactor(80);

            entity.ToTable("TMOUNTITEMTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TMOUNTTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.wMountID })
                .HasName("PK__TMOUNTTA__F886F34849F77EA5")
                .HasFillFactor(80);

            entity.ToTable("TMOUNTTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TNPCCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TNPCCHAR__30FE5743662D8D40")
                .HasFillFactor(80);

            entity.ToTable("TNPCCHART");

            entity.HasIndex(e => e.szName, "IX_TNPCCHART_NAME").HasFillFactor(80);

            entity.Property(e => e.wID).ValueGeneratedNever();
            entity.Property(e => e.NC_szName2)
                .HasMaxLength(50)
                .IsUnicode(false)
                .HasDefaultValue("_");
            entity.Property(e => e.szLocal)
                .HasMaxLength(100)
                .IsUnicode(false)
                .HasDefaultValue("");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TNPCITEMCHART>(entity =>
        {
            entity.HasKey(e => new { e.wNpcID, e.dwItemID })
                .HasName("PK__TNPCITEM__9070BB85190E12C8")
                .HasFillFactor(80);

            entity.ToTable("TNPCITEMCHART");

            entity.HasIndex(e => e.wNpcID, "IX_TNPCITEMCHART_NPC").HasFillFactor(80);
        });

        modelBuilder.Entity<TOPERATORTABLE>(entity =>
        {
            entity.HasKey(e => e.dwOperatorID)
                .HasName("PK__TOPERATO__9977503C5344D5FE")
                .HasFillFactor(80);

            entity.ToTable("TOPERATORTABLE");

            entity.Property(e => e.dwOperatorID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TPETCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TPETCHAR__30FE5743F9A8A457")
                .HasFillFactor(80);

            entity.ToTable("TPETCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TPETTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.wPetID })
                .HasName("PK__TPETTABL__A41333D05AE5F7C6")
                .HasFillFactor(80);

            entity.ToTable("TPETTABLE");

            entity.HasIndex(e => e.dwUserID, "IX_TPETTABLE_USER").HasFillFactor(80);

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.timeUse).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TPLAYTIMETABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TPLAYTIMETABLE");
        });

        modelBuilder.Entity<TPORTALCHART>(entity =>
        {
            entity.HasKey(e => e.wPortalID)
                .HasName("PK__TPORTALC__827BB9F07B28AA26")
                .HasFillFactor(80);

            entity.ToTable("TPORTALCHART");

            entity.Property(e => e.wPortalID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TPOSTERRORTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TPOSTERRORTABLE");
        });

        modelBuilder.Entity<TPOSTITEMTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TPOSTITEMTABLE");

            entity.HasIndex(e => e.dwPostID, "IX_TPOSTITEMTABLE").HasFillFactor(80);

            entity.HasIndex(e => e.dwCharID, "IX_TPOSTITEMTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TPOSTTABLE>(entity =>
        {
            entity.HasKey(e => e.dwPostID)
                .HasName("PK__TPOSTTAB__8F66CCFF65638639")
                .HasFillFactor(80);

            entity.ToTable("TPOSTTABLE");

            entity.HasIndex(e => new { e.dwCharID, e.dwPostID }, "IX_TPOSTTABLE_").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwCharID, e.bRead, e.bType, e.timeRecv }, "IX_TPOSTTABLE_CHARID_READ_TYPE_TIMERECV_").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwSendID, e.bType }, "IX_TPOSTTABLE_SENDID_").HasFillFactor(80);

            entity.Property(e => e.szMessage).HasMaxLength(2048);
            entity.Property(e => e.szRecvName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szSender).HasMaxLength(50);
            entity.Property(e => e.szTitle).HasMaxLength(256);
            entity.Property(e => e.timeRecv).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TPREMIUMSKILLCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TPREMIUMSKILLCHART");
        });

        modelBuilder.Entity<TPROTECTEDTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.dwProtected })
                .HasName("PK__TPROTECT__FDF5123F6934171D")
                .HasFillFactor(80);

            entity.ToTable("TPROTECTEDTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TPROTECTEDTABLE_CHAR").HasFillFactor(80);

            entity.Property(e => e.bOption).HasDefaultValue((byte)1);
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TPVPOINTCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TPVPOINTCHART");
        });

        modelBuilder.Entity<TPVPOINTTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TPVPOINT__B92C9D036EECF073")
                .HasFillFactor(80);

            entity.ToTable("TPVPOINTTABLE");

            entity.HasIndex(e => e.dwTotalPoint, "IX_TPVPOINTTABLE").HasFillFactor(80);

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TPVPRECENTTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TPVPRECENTTABLE");

            entity.HasIndex(e => new { e.dwCharID, e.dlDate }, "IX_TPVPRECENTTABLE").HasFillFactor(80);

            entity.Property(e => e.dlDate).HasColumnType("smalldatetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TPVPRECORDTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TPVPRECO__B92C9D0373B1A590")
                .HasFillFactor(80);

            entity.ToTable("TPVPRECORDTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TQCLASSCHART>(entity =>
        {
            entity.HasKey(e => e.dwClassID)
                .HasName("PK__TQCLASSC__B35837B17B08AE95")
                .HasFillFactor(80);

            entity.ToTable("TQCLASSCHART");

            entity.Property(e => e.dwClassID).ValueGeneratedNever();
            entity.Property(e => e.szNAME)
                .HasMaxLength(1024)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TQCONDITIONCHART>(entity =>
        {
            entity.HasKey(e => e.dwID).HasName("PK__TQCONDIT__2F7525020E5B7A2B");

            entity.ToTable("TQCONDITIONCHART");

            entity.HasIndex(e => e.dwQuestID, "IX_TQCONDITIONCHART_QUEST");

            entity.Property(e => e.dwID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TQREWARDCHART>(entity =>
        {
            entity.HasKey(e => e.dwID).HasName("PK__TQREWARD__2F75250214145381");

            entity.ToTable("TQREWARDCHART");

            entity.HasIndex(e => e.dwQuestID, "IX_TQREWARDCHART_QUEST");

            entity.Property(e => e.dwID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TQTITLECHART>(entity =>
        {
            entity.HasKey(e => e.dwQuestID)
                .HasName("PK__TQTITLEC__529A79CE039DF496")
                .HasFillFactor(80);

            entity.ToTable("TQTITLECHART");

            entity.Property(e => e.dwQuestID).ValueGeneratedNever();
            entity.Property(e => e.szAccept)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szComplete)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szMessage)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szNPCName)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szReject)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szReply)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szSummary)
                .HasMaxLength(1024)
                .IsUnicode(false);
            entity.Property(e => e.szTitle)
                .HasMaxLength(255)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TQUESTCHART>(entity =>
        {
            entity.HasKey(e => e.dwQuestID).HasName("PK__TQUESTCH__529A79CE19CD2CD7");

            entity.ToTable("TQUESTCHART");

            entity.HasIndex(e => e.dwParentID, "IX_TQUESTCHART_PARENT");

            entity.Property(e => e.dwQuestID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TQUESTITEMCHART>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TQUESTIT__2F752502314FB0AD")
                .HasFillFactor(80);

            entity.ToTable("TQUESTITEMCHART");

            entity.Property(e => e.dwID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TQUESTTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.dwQuestID })
                .HasName("PK__TQUESTTA__4C053A9F29199208")
                .HasFillFactor(80);

            entity.ToTable("TQUESTTABLE");

            entity.HasIndex(e => new { e.dwQuestID, e.bCompleteCount }, "IX_TQUESTTABLE").HasFillFactor(80);

            entity.HasIndex(e => e.dwCharID, "IX_TQUESTTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TQUESTTERMCHART>(entity =>
        {
            entity.HasKey(e => e.dwID).HasName("PK__TQUESTTE__2F7525021D9DBDBB");

            entity.ToTable("TQUESTTERMCHART");

            entity.HasIndex(e => e.dwQuestID, "IX_TQUESTTERMCHART_QUEST");

            entity.Property(e => e.dwID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TQUESTTERMTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.dwQuestID, e.dwTermID })
                .HasName("PK__TQUESTTE__A60E15C530BAB3D0")
                .HasFillFactor(80);

            entity.ToTable("TQUESTTERMTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TQUESTTERMTABLE_CHAR").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwCharID, e.dwQuestID, e.dwTermID }, "IX_TQUESTTERMTABLE_CHARID_QUESTID_TERMID").HasFillFactor(80);
        });

        modelBuilder.Entity<TRACECHART>(entity =>
        {
            entity.HasKey(e => e.bRaceID)
                .HasName("PK__TRACECHA__886B2FD2348B44B4")
                .HasFillFactor(80);

            entity.ToTable("TRACECHART");
        });

        modelBuilder.Entity<TRANKING>(entity =>
        {
            entity.HasKey(e => e.dwCharID).HasName("PK__TRANKING__B92C9D036B4AD28E");

            entity.ToTable("TRANKING");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TRECALLMAINTAINTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TRECALLMAINTAINTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TRECALLMAINTAINTABLE").HasFillFactor(80);

            entity.Property(e => e.bHostType)
                .HasMaxLength(10)
                .IsUnicode(false)
                .IsFixedLength();
        });

        modelBuilder.Entity<TRECALLMONTABLE>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TRECALLM__2F752502394FF9D1")
                .HasFillFactor(80);

            entity.ToTable("TRECALLMONTABLE");

            entity.HasIndex(e => e.dwOwnerID, "IX_TRECALLMONTABLE_OWNER").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwOwnerID, e.dwID }, "IX_TRECALLMONTABLE_OWNERID_ID").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwOwnerID, e.bEffect }, "IX_TRECALLMONTABLE_OWNERID_UPDATE").HasFillFactor(80);

            entity.Property(e => e.dwID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TRESERVEDPOST>(entity =>
        {
            entity.HasKey(e => e.dwSeq)
                .HasName("PK__TRESERVE__E15A5BA13FFCF760")
                .HasFillFactor(80);

            entity.ToTable("TRESERVEDPOST");

            entity.HasIndex(e => e.bSend, "IX_TRESERVEDPOST").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwRecverID, e.wItemID }, "IX_TRESERVEDPOST_1").HasFillFactor(80);

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.szMessage)
                .HasMaxLength(2048)
                .IsUnicode(false);
            entity.Property(e => e.szSender)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szTitle)
                .HasMaxLength(256)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TRPSGAMECHART>(entity =>
        {
            entity.HasKey(e => new { e.bType, e.bWinCount })
                .HasName("PK__TRPSGAME__59AC6B660F0FA742")
                .HasFillFactor(80);

            entity.ToTable("TRPSGAMECHART");
        });

        modelBuilder.Entity<TRPSGAMERECORDTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TRPSGAMERECORDTABLE");

            entity.HasIndex(e => new { e.bType, e.bWinCount, e.dWinDate }, "IX_TRPSGAMERECORDTABLE").HasFillFactor(80);

            entity.Property(e => e.dWinDate).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TSAVEDCUSTOMCLOAKTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSAVEDCUSTOMCLOAKTABLE");
        });

        modelBuilder.Entity<TSKILLCHART>(entity =>
        {
            entity.HasKey(e => e.wID).HasName("PK__TSKILLCH__30FE574364EF5044");

            entity.ToTable("TSKILLCHART");

            entity.HasIndex(e => new { e.dwClassID, e.bCanLearn, e.bLevel }, "IX_TSKILLCHART_CLASSID_CANLEARN_LEVEL");

            entity.Property(e => e.wID).ValueGeneratedNever();
            entity.Property(e => e.bRepeatCount).IsSparse();
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TSKILLDATum>(entity =>
        {
            entity.HasKey(e => new { e.wSkillID, e.bAction, e.bType, e.bAttr, e.bExec }).HasName("PK__TSKILLDA__2D89BDBB69B40561");

            entity.ToTable("TSKILLDATA");
        });

        modelBuilder.Entity<TSKILLLOG>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSKILLLOG");

            entity.Property(e => e.timeInsert).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TSKILLMAINTAINTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.wSkillID })
                .HasName("PK__TSKILLMA__80F81617652E7C0F")
                .HasFillFactor(80);

            entity.ToTable("TSKILLMAINTAINTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TSKILLMAINTAINTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TSKILLPOINTCHART>(entity =>
        {
            entity.HasKey(e => new { e.wID, e.bLevel })
                .HasName("PK__TSKILLPO__8AD7F2AF17A4ED43")
                .HasFillFactor(80);

            entity.ToTable("TSKILLPOINTCHART");
        });

        modelBuilder.Entity<TSKILLREWARD>(entity =>
        {
            entity.HasKey(e => new { e.wID, e.bLevel })
                .HasName("PK__TSKILLRE__8AD7F2AF1A8159EE")
                .HasFillFactor(80);

            entity.ToTable("TSKILLREWARD");
        });

        modelBuilder.Entity<TSKILLREWARDMONEY>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSKILLREWARDMONEY");
        });

        modelBuilder.Entity<TSKILLTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSKILLTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TSKILLTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TSKYGARDENTABLE>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TSKYGARD__30FE574313F457F0")
                .HasFillFactor(80);

            entity.ToTable("TSKYGARDENTABLE");

            entity.Property(e => e.wID).ValueGeneratedNever();
            entity.Property(e => e.dateWarTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TSOULMATETABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TSOULMAT__B92C9D037A2998F5")
                .HasFillFactor(80);

            entity.ToTable("TSOULMATETABLE");

            entity.HasIndex(e => e.dwTarget, "IX_TSOULMATETABLE").HasFillFactor(80);

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TSPAWNPATHCHART>(entity =>
        {
            entity.HasKey(e => new { e.wSpawnID, e.bPathID })
                .HasName("PK__TSPAWNPA__FE27189F17C4E8D4")
                .HasFillFactor(80);

            entity.ToTable("TSPAWNPATHCHART");
        });

        modelBuilder.Entity<TSPAWNPOSCHART>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TSPAWNPO__30FE57431B9579B8")
                .HasFillFactor(80);

            entity.ToTable("TSPAWNPOSCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TSPECIALBOXCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSPECIALBOXCHART");

            entity.Property(e => e.bClass)
                .HasMaxLength(1)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TSTARTHOTKEY>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSTARTHOTKEY");

            entity.HasIndex(e => e.bClassID, "IX_TSTARTHOTKEY").HasFillFactor(80);
        });

        modelBuilder.Entity<TSTARTITEMCHART>(entity =>
        {
            entity.HasKey(e => new { e.bCountry, e.bClass, e.bInven, e.bSlot })
                .HasName("PK__TSTARTIT__3634486623169FEF")
                .HasFillFactor(80);

            entity.ToTable("TSTARTITEMCHART");
        });

        modelBuilder.Entity<TSTARTRECALL>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSTARTRECALL");

            entity.HasIndex(e => e.bClassID, "IX_TSTARTRECALL").HasFillFactor(80);
        });

        modelBuilder.Entity<TSTARTSKILL>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSTARTSKILL");

            entity.HasIndex(e => e.bClassID, "IX_TSTARTSKILL").HasFillFactor(80);
        });

        modelBuilder.Entity<TSVRCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TSVRCHART");
        });

        modelBuilder.Entity<TSVRMSGCHART>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TSVRMSGC__2F7525020D3C6D69")
                .HasFillFactor(80);

            entity.ToTable("TSVRMSGCHART");

            entity.Property(e => e.dwID).ValueGeneratedNever();
            entity.Property(e => e.szMessage)
                .HasMaxLength(256)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TSWITCHCHART>(entity =>
        {
            entity.HasKey(e => e.dwSwitchID)
                .HasName("PK__TSWITCHC__45C8B3693EFEB186")
                .HasFillFactor(80);

            entity.ToTable("TSWITCHCHART");

            entity.Property(e => e.dwSwitchID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TTAXTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TTAXTABLE");
        });

        modelBuilder.Entity<TTEMPCABINETITEMTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bCabinetID, e.dwStItemID })
                .HasName("PK__TTEMPCAB__A2C2BB4D17B9FBDC")
                .HasFillFactor(80);

            entity.ToTable("TTEMPCABINETITEMTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TTEMPCABINETITEMTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TTEMPCABINETTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bCabinetID })
                .HasName("PK__TTEMPCAB__83E1E3EF1C7EB0F9")
                .HasFillFactor(80);

            entity.ToTable("TTEMPCABINETTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TTEMPCABINETTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TTEMPEXPITEMTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TTEMPEXPITEMTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TTEMPINVENTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.bInvenID })
                .HasName("PK__TTEMPINV__688A92BB21436616")
                .HasFillFactor(80);

            entity.ToTable("TTEMPINVENTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TTEMPINVENTABLE_CHAR").HasFillFactor(80);

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TTEMPITEMTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TTEMPITEMTABLE");

            entity.HasIndex(e => e.dlID, "IX_TTEMPITEMTABLE").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwOwnerID, e.bOwnerType, e.bStorageType }, "IX_TTEMPITEMTABLE_CHAR").HasFillFactor(80);

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TTEMPITEMUSEDTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.wDelayGroupID })
                .HasName("PK__TTEMPITE__A4A3CF4228E487DE")
                .HasFillFactor(80);

            entity.ToTable("TTEMPITEMUSEDTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TTEMPITEMUSEDTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TTEMPSKILLMAINTAINTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.wSkillID })
                .HasName("PK__TTEMPSKI__80F816172CB518C2")
                .HasFillFactor(80);

            entity.ToTable("TTEMPSKILLMAINTAINTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TTEMPSKILLMAINTAINTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TTEMPSKILLTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.wSkillID })
                .HasName("PK__TTEMPSKI__80F81617363E82FC")
                .HasFillFactor(80);

            entity.ToTable("TTEMPSKILLTABLE");

            entity.HasIndex(e => e.dwCharID, "IX_TTEMPSKILLTABLE_CHAR").HasFillFactor(80);
        });

        modelBuilder.Entity<TTITLECHART>(entity =>
        {
            entity.HasKey(e => e.wTitleID)
                .HasName("PK__TTITLECH__37CDA9CA42CF426A")
                .HasFillFactor(80);

            entity.ToTable("TTITLECHART");

            entity.Property(e => e.wTitleID).ValueGeneratedNever();
            entity.Property(e => e.strTitle).HasColumnType("text");
        });

        modelBuilder.Entity<TTITLETABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwCharID, e.wTitleID })
                .HasName("PK__TTITLETA__0A50479F28EF74D6")
                .HasFillFactor(80);

            entity.ToTable("TTITLETABLE");
        });

        modelBuilder.Entity<TTNMTEVENTREWARDTABLE>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TTNMTEVE__30FE57433A0F13E0")
                .HasFillFactor(80);

            entity.ToTable("TTNMTEVENTREWARDTABLE");
        });

        modelBuilder.Entity<TTNMTEVENTSCHEDULETABLE>(entity =>
        {
            entity.HasKey(e => new { e.wTournamentID, e.bStep })
                .HasName("PK__TTNMTEVE__9D19B92A3DDFA4C4")
                .HasFillFactor(80);

            entity.ToTable("TTNMTEVENTSCHEDULETABLE");
        });

        modelBuilder.Entity<TTNMTEVENTTABLE>(entity =>
        {
            entity.HasKey(e => new { e.wTournamentID, e.bEntryID })
                .HasName("PK__TTNMTEVE__4BF8235C41B035A8")
                .HasFillFactor(80);

            entity.ToTable("TTNMTEVENTTABLE");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TTNMTEVENTTIMETABLE>(entity =>
        {
            entity.HasKey(e => e.wTournamentID)
                .HasName("PK__TTNMTEVE__FF5D6FA14580C68C")
                .HasFillFactor(80);

            entity.ToTable("TTNMTEVENTTIMETABLE");

            entity.Property(e => e.wTournamentID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TTOURNAMENTCHART>(entity =>
        {
            entity.HasKey(e => e.bEntryID)
                .HasName("PK__TTOURNAM__4A54CFDA49515770")
                .HasFillFactor(80);

            entity.ToTable("TTOURNAMENTCHART");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TTOURNAMENTPLAYERTABLE>(entity =>
        {
            entity.HasKey(e => e.dwCharID)
                .HasName("PK__TTOURNAM__B92C9D034F0A30C6")
                .HasFillFactor(80);

            entity.ToTable("TTOURNAMENTPLAYERTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedNever();
            entity.Property(e => e.szHWID)
                .HasMaxLength(55)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TTOURNAMENTREWARDCHART>(entity =>
        {
            entity.HasKey(e => e.wID).HasName("PK__TTOURNAM__30FE574345E0B93F");

            entity.ToTable("TTOURNAMENTREWARDCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TTOURNAMENTREWARDCHART_weap>(entity =>
        {
            entity.HasKey(e => e.wID)
                .HasName("PK__TTOURNAM__30FE574356AB528E")
                .HasFillFactor(80);

            entity.ToTable("TTOURNAMENTREWARDCHART_weap");

            entity.Property(e => e.wID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TTOURNAMENTSCHEDULECHART>(entity =>
        {
            entity.HasKey(e => new { e.bStep, e.bGroup })
                .HasName("PK__TTOURNAM__1137A6565A7BE372")
                .HasFillFactor(80);

            entity.ToTable("TTOURNAMENTSCHEDULECHART");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TTOURNAMENTSTATUSTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TTOURNAMENTSTATUSTABLE");
        });

        modelBuilder.Entity<TUNIFYPET>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.wPetID })
                .HasName("PK__TUNIFYPE__A41333D028CF7945")
                .HasFillFactor(80);

            entity.ToTable("TUNIFYPET");
        });

        modelBuilder.Entity<TUNITCHART>(entity =>
        {
            entity.HasKey(e => new { e.bGroup, e.bServerID, e.wMapID, e.wUnitID })
                .HasName("PK__TUNITCHA__7F71EF1B3B38E447")
                .HasFillFactor(80);

            entity.ToTable("TUNITCHART");

            entity.HasIndex(e => new { e.bGroup, e.wMapID, e.bServerID }, "IX_TUNITCHART_GR_SVR_MAP_ID").HasFillFactor(80);
        });

        modelBuilder.Entity<TVIEW_CASHCATEGORYCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_CASHCATEGORYCHART");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TVIEW_CASHGAMBLECHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_CASHGAMBLECHART");
        });

        modelBuilder.Entity<TVIEW_CASHITEMCABINET>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_CASHITEMCABINET");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.dwID).ValueGeneratedOnAdd();
        });

        modelBuilder.Entity<TVIEW_CASHSHOPITEMCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_CASHSHOPITEMCHART");
        });

        modelBuilder.Entity<TVIEW_CHARTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_CHARTABLE");

            entity.Property(e => e.dwCharID).ValueGeneratedOnAdd();
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVIEW_DURINGITEMTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_DURINGITEMTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TVIEW_GUILDTACTICSTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_GUILDTACTICSTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVIEW_GUILDVOLUNTEERTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_GUILDVOLUNTEERTABLE");

            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVIEW_MENTORMEMBER>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_MENTORMEMBER");

            entity.Property(e => e.dLogoutDate).HasColumnType("smalldatetime");
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVIEW_MONTHRANK>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_MONTHRANK");

            entity.Property(e => e.szGuild)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szSay)
                .HasMaxLength(256)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVIEW_SKILLPOINT>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_SKILLPOINT");
        });

        modelBuilder.Entity<TVIEW_SOULMATE>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_SOULMATE");

            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVIEW_TOURNAMENTPLAYER>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("TVIEW_TOURNAMENTPLAYER");

            entity.Property(e => e.szHWID)
                .HasMaxLength(55)
                .IsUnicode(false);
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<charkilling_log>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("charkilling_log");

            entity.Property(e => e.date)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<dtproperty>(entity =>
        {
            entity.HasKey(e => new { e.id, e.property })
                .HasName("PK__dtproper__92EB783E65438AA8")
                .HasFillFactor(80);

            entity.Property(e => e.id).ValueGeneratedOnAdd();
            entity.Property(e => e.property)
                .HasMaxLength(64)
                .IsUnicode(false);
            entity.Property(e => e.lvalue).HasColumnType("image");
            entity.Property(e => e.uvalue).HasMaxLength(255);
            entity.Property(e => e.value)
                .HasMaxLength(255)
                .IsUnicode(false);
        });

        modelBuilder.Entity<tguildview>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("tguildview");

            entity.Property(e => e.guild_leader)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.guild_name)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<tviewguildplayer>(entity =>
        {
            entity
                .HasNoKey()
                .ToView("tviewguildplayers");

            entity.Property(e => e.g_name)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.p_name)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        OnModelCreatingPartial(modelBuilder);
    }

    partial void OnModelCreatingPartial(ModelBuilder modelBuilder);
}
