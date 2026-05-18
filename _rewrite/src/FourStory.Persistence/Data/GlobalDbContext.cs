using System;
using System.Collections.Generic;
using FourStory.Persistence.Global;
using Microsoft.EntityFrameworkCore;

namespace FourStory.Persistence;

public partial class GlobalDbContext : DbContext
{
    public GlobalDbContext(DbContextOptions<GlobalDbContext> options)
        : base(options)
    {
    }

    public virtual DbSet<IPBLACKLIST> IPBLACKLISTs { get; set; }

    public virtual DbSet<IPBLACKLIST_game> IPBLACKLIST_games { get; set; }

    public virtual DbSet<TACCOUNT> TACCOUNTs { get; set; }

    public virtual DbSet<TACCOUNT_PW> TACCOUNT_PWs { get; set; }

    public virtual DbSet<TALLCHARTABLE> TALLCHARTABLEs { get; set; }

    public virtual DbSet<TALLCHARTABLE_TRIGGER> TALLCHARTABLE_TRIGGERs { get; set; }

    public virtual DbSet<TALLLOCALTABLE> TALLLOCALTABLEs { get; set; }

    public virtual DbSet<TALLRANKGUILDTABLE> TALLRANKGUILDTABLEs { get; set; }

    public virtual DbSet<TALLRANKTABLE> TALLRANKTABLEs { get; set; }

    public virtual DbSet<TCABINETITEMCHART> TCABINETITEMCHARTs { get; set; }

    public virtual DbSet<TCASHBONUSITEMCHART> TCASHBONUSITEMCHARTs { get; set; }

    public virtual DbSet<TCASHCATEGORYCHART> TCASHCATEGORYCHARTs { get; set; }

    public virtual DbSet<TCASHGAMBLECHART> TCASHGAMBLECHARTs { get; set; }

    public virtual DbSet<TCASHITEMBUYTABLE> TCASHITEMBUYTABLEs { get; set; }

    public virtual DbSet<TCASHITEMCABINETTABLE> TCASHITEMCABINETTABLEs { get; set; }

    public virtual DbSet<TCASHITEMCABINETTABLE_PW> TCASHITEMCABINETTABLE_PWs { get; set; }

    public virtual DbSet<TCASHITEMCABINETTABLE_temp> TCASHITEMCABINETTABLE_temps { get; set; }

    public virtual DbSet<TCASHITEMINFOCHART> TCASHITEMINFOCHARTs { get; set; }

    public virtual DbSet<TCASHITEMPRICETABLE> TCASHITEMPRICETABLEs { get; set; }

    public virtual DbSet<TCASHSHOPITEMCHART> TCASHSHOPITEMCHARTs { get; set; }

    public virtual DbSet<TCASHTESTTABLE> TCASHTESTTABLEs { get; set; }

    public virtual DbSet<TCHANNEL> TCHANNELs { get; set; }

    public virtual DbSet<TCURRENTUSER> TCURRENTUSERs { get; set; }

    public virtual DbSet<TDURINGITEMTABLE> TDURINGITEMTABLEs { get; set; }

    public virtual DbSet<TEMPUSERID> TEMPUSERIDs { get; set; }

    public virtual DbSet<TEVENT042501TL> TEVENT042501TLs { get; set; }

    public virtual DbSet<TEVENTCHART> TEVENTCHARTs { get; set; }

    public virtual DbSet<TEVENTGIVEERROR> TEVENTGIVEERRORs { get; set; }

    public virtual DbSet<TEVENTHORSE> TEVENTHORSEs { get; set; }

    public virtual DbSet<TGROUP> TGROUPs { get; set; }

    public virtual DbSet<TIPADDR> TIPADDRs { get; set; }

    public virtual DbSet<TIPAUTHORITY> TIPAUTHORITies { get; set; }

    public virtual DbSet<TITEMCHART> TITEMCHARTs { get; set; }

    public virtual DbSet<TKEEPINGNAME> TKEEPINGNAMEs { get; set; }

    public virtual DbSet<TLIMITEDLEVELCHART> TLIMITEDLEVELCHARTs { get; set; }

    public virtual DbSet<TLOG> TLOGs { get; set; }

    public virtual DbSet<TLogTest> TLogTests { get; set; }

    public virtual DbSet<TMACHINE> TMACHINEs { get; set; }

    public virtual DbSet<TMANAGER> TMANAGERs { get; set; }

    public virtual DbSet<TMANAGERLOG> TMANAGERLOGs { get; set; }

    public virtual DbSet<TMONSTERCHART> TMONSTERCHARTs { get; set; }

    public virtual DbSet<TNETWORK> TNETWORKs { get; set; }

    public virtual DbSet<TPCBANGMASTERTABLE> TPCBANGMASTERTABLEs { get; set; }

    public virtual DbSet<TPCBANGPLAYTABLE> TPCBANGPLAYTABLEs { get; set; }

    public virtual DbSet<TPREVERSION> TPREVERSIONs { get; set; }

    public virtual DbSet<TREGIONCHART> TREGIONCHARTs { get; set; }

    public virtual DbSet<TRESERVEDNAME> TRESERVEDNAMEs { get; set; }

    public virtual DbSet<TSECURECODE> TSECURECODEs { get; set; }

    public virtual DbSet<TSERVER> TSERVERs { get; set; }

    public virtual DbSet<TSMSTABLE> TSMSTABLEs { get; set; }

    public virtual DbSet<TSVRTYPE> TSVRTYPEs { get; set; }

    public virtual DbSet<TTEMPCASHITEM> TTEMPCASHITEMs { get; set; }

    public virtual DbSet<TTEMPDURINGITEMTABLE> TTEMPDURINGITEMTABLEs { get; set; }

    public virtual DbSet<TTESTLOGINUSER> TTESTLOGINUSERs { get; set; }

    public virtual DbSet<TUNIFYCASHITEM> TUNIFYCASHITEMs { get; set; }

    public virtual DbSet<TUNIFYPOSTITEMTABLE> TUNIFYPOSTITEMTABLEs { get; set; }

    public virtual DbSet<TUSERATTENDTABLE> TUSERATTENDTABLEs { get; set; }

    public virtual DbSet<TUSERINFOTABLE> TUSERINFOTABLEs { get; set; }

    public virtual DbSet<TUSERPROTECTED> TUSERPROTECTEDs { get; set; }

    public virtual DbSet<TUSERPROTECTED_old> TUSERPROTECTED_olds { get; set; }

    public virtual DbSet<TUSER_INTERFACE> TUSER_INTERFACEs { get; set; }

    public virtual DbSet<TVERSION> TVERSIONs { get; set; }

    public virtual DbSet<TVETERANCHART> TVETERANCHARTs { get; set; }

    public virtual DbSet<TempEvent> TempEvents { get; set; }

    public virtual DbSet<USERIPLOG> USERIPLOGs { get; set; }

    public virtual DbSet<releaseDate> releaseDates { get; set; }

    public virtual DbSet<ttemplevel> ttemplevels { get; set; }

    public virtual DbSet<ttempuser> ttempusers { get; set; }

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        modelBuilder.UseCollation("Latin1_General_CI_AS_KS");

        modelBuilder.Entity<IPBLACKLIST>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("IPBLACKLIST");

            entity.Property(e => e.szIP)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<IPBLACKLIST_game>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("IPBLACKLIST_game");

            entity.Property(e => e.szIP)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TACCOUNT>(entity =>
        {
            entity.HasKey(e => e.dwUserID).HasFillFactor(80);

            entity.ToTable("TACCOUNT");

            entity.HasIndex(e => e.szUserID, "IX_TACCOUNT_USERID").HasFillFactor(80);

            entity.Property(e => e.bCheck).HasDefaultValue((byte)0, "DF_TACCOUNT_bCheckIP");
            entity.Property(e => e.dFirstLogin).HasColumnType("smalldatetime");
            entity.Property(e => e.dLastLogin).HasColumnType("smalldatetime");
            entity.Property(e => e.szPasswd)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szUserID)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TACCOUNT_PW>(entity =>
        {
            entity.HasKey(e => e.dwUserID)
                .HasName("PK__TACCOUNT__7F8566D75A6F5FCC")
                .HasFillFactor(80);

            entity.ToTable("TACCOUNT_PW");

            entity.Property(e => e.bCheck).HasDefaultValue((byte)0);
            entity.Property(e => e.dFirstLogin).HasColumnType("smalldatetime");
            entity.Property(e => e.dLastLogin).HasColumnType("smalldatetime");
            entity.Property(e => e.dwEMKey).HasColumnType("text");
            entity.Property(e => e.dwPWKey).HasColumnType("text");
            entity.Property(e => e.szOrigin)
                .HasMaxLength(4)
                .IsUnicode(false);
            entity.Property(e => e.szPasswd)
                .HasMaxLength(255)
                .IsUnicode(false);
            entity.Property(e => e.szRealUsername)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szUserID)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TALLCHARTABLE>(entity =>
        {
            entity.HasKey(e => e.dwSeq)
                .HasName("PK__TALLCHAR__E15A5BA12D9CB955")
                .HasFillFactor(80);

            entity.ToTable("TALLCHARTABLE", tb =>
                {
                    tb.HasTrigger("TALLCHARTABLE_TRIGGER_DELETE_copy");
                    tb.HasTrigger("TALLCHARTABLE_TRIGGER_INSERT_copy");
                    tb.HasTrigger("TALLCHARTABLE_TRIGGER_UPDATE_copy");
                });

            entity.HasIndex(e => new { e.dwUserID, e.bDelete }, "IX_TALLCHARTABLE_USERID_DELETE").HasFillFactor(80);

            entity.HasIndex(e => new { e.dwUserID, e.bWorldID }, "IX_TALLCHARTABLE_USERID_WORLD").HasFillFactor(80);

            entity.HasIndex(e => new { e.bWorldID, e.dwCharID }, "IX_TALLCHARTABLE_WORLD_CHARID").HasFillFactor(80);

            entity.Property(e => e.BanReason).HasColumnType("text");
            entity.Property(e => e.dCreateDate)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("datetime");
            entity.Property(e => e.dDeleteDate).HasColumnType("datetime");
            entity.Property(e => e.dLoginDate).HasColumnType("datetime");
            entity.Property(e => e.dLogoutDate).HasColumnType("datetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TALLCHARTABLE_TRIGGER>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TALLCHARTABLE_TRIGGER");

            entity.HasIndex(e => e.dwSeq, "IX_TALLCHARTABLE_TRIGGER_SEQ").HasFillFactor(80);

            entity.Property(e => e.dOPDate)
                .HasDefaultValueSql("(getdate())", "DF_TALLCHARTABLE_TRIGGER_dOPDate")
                .HasColumnType("datetime");
            entity.Property(e => e.szDBOP)
                .HasMaxLength(1)
                .IsUnicode(false)
                .IsFixedLength()
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TALLLOCALTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dDate, e.bWorld, e.wLocalID }).HasFillFactor(80);

            entity.ToTable("TALLLOCALTABLE");

            entity.Property(e => e.dDate).HasColumnType("smalldatetime");
            entity.Property(e => e.dDefend).HasColumnType("smalldatetime");
            entity.Property(e => e.dOccupy).HasColumnType("smalldatetime");
            entity.Property(e => e.szGuildName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szLocalName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TALLRANKGUILDTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TALLRANKGUILDTABLE");

            entity.Property(e => e.dDate).HasColumnType("smalldatetime");
            entity.Property(e => e.dwSeq).ValueGeneratedOnAdd();
            entity.Property(e => e.szChiefName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szGuildName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.timeEstablish).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TALLRANKTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dDate, e.bWorld, e.bRankType, e.dwCharID }).HasFillFactor(80);

            entity.ToTable("TALLRANKTABLE");

            entity.Property(e => e.dDate).HasColumnType("smalldatetime");
            entity.Property(e => e.dwSeq).ValueGeneratedOnAdd();
            entity.Property(e => e.szCharName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TCABINETITEMCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCABINETITEMCHART");
        });

        modelBuilder.Entity<TCASHBONUSITEMCHART>(entity =>
        {
            entity.HasKey(e => new { e.wCashItemID, e.wBonusItem }).HasFillFactor(80);

            entity.ToTable("TCASHBONUSITEMCHART");
        });

        modelBuilder.Entity<TCASHCATEGORYCHART>(entity =>
        {
            entity.HasKey(e => e.bID).HasFillFactor(80);

            entity.ToTable("TCASHCATEGORYCHART");

            entity.Property(e => e.szName).HasMaxLength(50);
        });

        modelBuilder.Entity<TCASHGAMBLECHART>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TCASHGAM__2F75250266A02C87")
                .HasFillFactor(80);

            entity.ToTable("TCASHGAMBLECHART");

            entity.Property(e => e.dwID).ValueGeneratedNever();
        });

        modelBuilder.Entity<TCASHITEMBUYTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCASHITEMBUYTABLE");

            entity.Property(e => e.dDate).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TCASHITEMCABINETTABLE>(entity =>
        {
            entity.HasKey(e => e.dwID).HasFillFactor(80);

            entity.ToTable("TCASHITEMCABINETTABLE");

            entity.HasIndex(e => e.dwUserID, "IX_TCASHITEMCABINETTABLE").HasFillFactor(80);

            entity.HasIndex(e => e.bWorldID, "IX_TCASHITEMCABINETTABLE_1").HasFillFactor(80);

            entity.HasIndex(e => e.dlID, "IX_TCASHITEMCABINETTABLE_2").HasFillFactor(80);

            entity.HasIndex(e => e.wItemID, "IX_TCASHITEMCABINETTABLE_3").HasFillFactor(80);

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TCASHITEMCABINETTABLE_PW>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TCASHITE__2F7525024297D63B")
                .HasFillFactor(80);

            entity.ToTable("TCASHITEMCABINETTABLE_PW");

            entity.HasIndex(e => e.dwUserID, "IX_TCASHITEMCABINETTABLE").HasFillFactor(80);

            entity.HasIndex(e => e.bWorldID, "IX_TCASHITEMCABINETTABLE_1").HasFillFactor(80);

            entity.HasIndex(e => e.dlID, "IX_TCASHITEMCABINETTABLE_2").HasFillFactor(80);

            entity.HasIndex(e => e.wItemID, "IX_TCASHITEMCABINETTABLE_3").HasFillFactor(80);

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TCASHITEMCABINETTABLE_temp>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCASHITEMCABINETTABLE_temp");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
            entity.Property(e => e.dwID).ValueGeneratedOnAdd();
        });

        modelBuilder.Entity<TCASHITEMINFOCHART>(entity =>
        {
            entity.HasKey(e => e.wCashItemID).HasFillFactor(80);

            entity.ToTable("TCASHITEMINFOCHART");

            entity.Property(e => e.wCashItemID).ValueGeneratedNever();
            entity.Property(e => e.szExplain)
                .HasMaxLength(500)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS")
                .HasDefaultValue("", "DF_TCASHITEMINFOCHART_szName");
            entity.Property(e => e.szUseTime)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TCASHITEMPRICETABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TCASHITEMPRICETABLE");
        });

        modelBuilder.Entity<TCASHSHOPITEMCHART>(entity =>
        {
            entity.HasKey(e => e.wID).HasFillFactor(80);

            entity.ToTable("TCASHSHOPITEMCHART");

            entity.Property(e => e.wID).ValueGeneratedNever();
            entity.Property(e => e.dLimitedEnd).HasColumnType("smalldatetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TCASHTESTTABLE>(entity =>
        {
            entity.HasKey(e => e.dwUserID)
                .HasName("PK__TCASHTESTTABLE__1F4E99FE")
                .HasFillFactor(80);

            entity.ToTable("TCASHTESTTABLE");

            entity.Property(e => e.dwUserID).ValueGeneratedNever();
            entity.Property(e => e.byWho).HasColumnType("text");
            entity.Property(e => e.isPosted).HasColumnType("datetime");
        });

        modelBuilder.Entity<TCHANNEL>(entity =>
        {
            entity.HasKey(e => new { e.bChannel, e.bGroupID })
                .HasName("PK__TCHANNEL__B2A02D6376177A41")
                .HasFillFactor(80);

            entity.ToTable("TCHANNEL");

            entity.HasIndex(e => e.bGroupID, "IX_TCHANNEL").HasFillFactor(80);

            entity.HasIndex(e => new { e.bChannel, e.bStatus }, "IX_TCHANNEL_CHANNEL_STATUS").HasFillFactor(80);

            entity.HasIndex(e => new { e.bGroupID, e.bChannel }, "IX_TCHANNEL_GROUP_CHANNEL").HasFillFactor(80);

            entity.HasIndex(e => new { e.bGroupID, e.bStatus }, "IX_TCHANNEL_GROUP_STATUS").HasFillFactor(80);

            entity.HasIndex(e => e.bStatus, "IX_TCHANNEL_STATUS").HasFillFactor(80);

            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.wBusy).HasDefaultValue((short)500);
            entity.Property(e => e.wFull).HasDefaultValue((short)5000);

            entity.HasOne(d => d.bGroup).WithMany(p => p.TCHANNELs)
                .HasForeignKey(d => d.bGroupID)
                .OnDelete(DeleteBehavior.ClientSetNull)
                .HasConstraintName("FK__TCHANNEL__bGroup__7ADC2F5E");
        });

        modelBuilder.Entity<TCURRENTUSER>(entity =>
        {
            entity.HasKey(e => e.dwKEY)
                .HasName("PK__TCURRENT__8B5BDC2F4A38F803")
                .HasFillFactor(80);

            entity.ToTable("TCURRENTUSER");

            entity.HasIndex(e => new { e.dwKEY, e.dwUserID, e.bLocked }, "IX_TCURRENTUSER_KEY_USERID_LOCK").HasFillFactor(80);

            entity.HasIndex(e => e.dwUserID, "IX_TCURRENTUSER_USERID").HasFillFactor(80);

            entity.Property(e => e.dEnterDate)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("datetime");
            entity.Property(e => e.dLoginDate)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("datetime");
            entity.Property(e => e.szIPAddr)
                .HasMaxLength(50)
                .IsUnicode(false);
            entity.Property(e => e.szLoginIP)
                .HasMaxLength(50)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TDURINGITEMTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.wItemID }).HasFillFactor(80);

            entity.ToTable("TDURINGITEMTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TEMPUSERID>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TEMPUSERID");
        });

        modelBuilder.Entity<TEVENT042501TL>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TEVENT042501TL");
        });

        modelBuilder.Entity<TEVENTCHART>(entity =>
        {
            entity.HasKey(e => e.dwIndex).HasFillFactor(80);

            entity.ToTable("TEVENTCHART");

            entity.Property(e => e.dwIndex).ValueGeneratedNever();
            entity.Property(e => e.dEndDate).HasColumnType("smalldatetime");
            entity.Property(e => e.dStartDate).HasColumnType("smalldatetime");
            entity.Property(e => e.szEndMsg)
                .HasMaxLength(1024)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szMidMsg)
                .HasMaxLength(1024)
                .IsUnicode(false)
                .HasDefaultValue("")
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szStartMsg)
                .HasMaxLength(1024)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szTitle)
                .HasMaxLength(256)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szValue)
                .HasMaxLength(1024)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TEVENTGIVEERROR>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TEVENTGIVEERROR");
        });

        modelBuilder.Entity<TEVENTHORSE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TEVENTHORSE");
        });

        modelBuilder.Entity<TGROUP>(entity =>
        {
            entity.HasKey(e => e.bGroupID).HasFillFactor(80);

            entity.ToTable("TGROUP");

            entity.HasIndex(e => e.bGroupID, "IX_TGROUP_GROUPID").HasFillFactor(80);

            entity.HasIndex(e => new { e.bGroupID, e.bStatus }, "IX_TGROUP_GROUPID_STATUS").HasFillFactor(80);

            entity.HasIndex(e => e.bStatus, "IX_TGROUP_STATUS").HasFillFactor(80);

            entity.Property(e => e.bUseRate).HasDefaultValue((byte)1, "DF_TGROUP_bUseRate");
            entity.Property(e => e.szDSN)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szPasswd)
                .HasMaxLength(255)
                .IsUnicode(false)
                .UseCollation("Latin1_General_BIN");
            entity.Property(e => e.szUserID)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.wBusy).HasDefaultValue((short)500, "DF_TGROUP_wBusy");
            entity.Property(e => e.wFull).HasDefaultValue((short)5000, "DF_TGROUP_wFull");
        });

        modelBuilder.Entity<TIPADDR>(entity =>
        {
            entity.HasKey(e => new { e.bMachineID, e.szIPAddr }).HasFillFactor(80);

            entity.ToTable("TIPADDR");

            entity.HasIndex(e => e.bMachineID, "IX_TIPADDR_MACHINEID").HasFillFactor(80);

            entity.HasIndex(e => new { e.bMachineID, e.bActive }, "IX_TIPADDR_MACHINEID_ACTIVE").HasFillFactor(80);

            entity.Property(e => e.szIPAddr)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szPriAddr)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS")
                .HasDefaultValue("", "DF_TIPADDR_szPriAddr");

            entity.HasOne(d => d.bMachine).WithMany(p => p.TIPADDRs)
                .HasForeignKey(d => d.bMachineID)
                .OnDelete(DeleteBehavior.ClientSetNull)
                .HasConstraintName("FK_TIPADDR_TMACHINE");
        });

        modelBuilder.Entity<TIPAUTHORITY>(entity =>
        {
            entity.HasKey(e => e.szIP).HasFillFactor(80);

            entity.ToTable("TIPAUTHORITY");

            entity.HasIndex(e => new { e.szIP, e.bAuthority }, "IX_TIPAUTHORITY_IP_AUTHORITY").HasFillFactor(80);

            entity.Property(e => e.szIP)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TITEMCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TITEMCHART");

            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TKEEPINGNAME>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TKEEPINGNAME");

            entity.HasIndex(e => e.szName, "IX_TKEEPINGNAME").HasFillFactor(80);

            entity.Property(e => e.dwSeq).ValueGeneratedOnAdd();
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TLIMITEDLEVELCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TLIMITEDLEVELCHART");
        });

        modelBuilder.Entity<TLOG>(entity =>
        {
            entity.HasKey(e => e.dwKEY).HasFillFactor(80);

            entity.ToTable("TLOG");

            entity.Property(e => e.dwKEY).ValueGeneratedNever();
            entity.Property(e => e.timeLOGIN).HasColumnType("smalldatetime");
            entity.Property(e => e.timeLOGOUT).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TLogTest>(entity =>
        {
            entity.HasKey(e => e.dwID)
                .HasName("PK__TLogTest__222B06A9")
                .HasFillFactor(80);

            entity.ToTable("TLogTest");

            entity.Property(e => e.szLOG)
                .HasMaxLength(255)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TMACHINE>(entity =>
        {
            entity.HasKey(e => e.bMachineID).HasFillFactor(80);

            entity.ToTable("TMACHINE");

            entity.Property(e => e.szNAME)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TMANAGER>(entity =>
        {
            entity.HasKey(e => e.szID)
                .HasName("PK__TMANAGER__1E5A75C5")
                .HasFillFactor(80);

            entity.ToTable("TMANAGER");

            entity.Property(e => e.szID)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.bOPAuthority).HasDefaultValue((byte)1, "DF_TMANAGER_bOPAuthority");
            entity.Property(e => e.dCreateDate).HasColumnType("smalldatetime");
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szOpratorCharID)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szPasswd)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szPhoneNum)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TMANAGERLOG>(entity =>
        {
            entity.HasKey(e => e.dwSeq)
                .HasName("PK__TMANAGERLOG__2042BE37")
                .HasFillFactor(80);

            entity.ToTable("TMANAGERLOG");

            entity.Property(e => e.dDate)
                .HasDefaultValueSql("(getdate())", "DF_TMANAGERLOG_dDate")
                .HasColumnType("datetime");
            entity.Property(e => e.szCommand)
                .HasMaxLength(20)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szGMID)
                .HasMaxLength(15)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szIP)
                .HasMaxLength(15)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szLog)
                .HasMaxLength(4000)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TMONSTERCHART>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TMONSTERCHART");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TNETWORK>(entity =>
        {
            entity.HasKey(e => e.bMachineID).HasFillFactor(80);

            entity.ToTable("TNETWORK");

            entity.Property(e => e.szNetwork)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");

            entity.HasOne(d => d.bMachine).WithOne(p => p.TNETWORK)
                .HasForeignKey<TNETWORK>(d => d.bMachineID)
                .OnDelete(DeleteBehavior.ClientSetNull)
                .HasConstraintName("FK_TNETWORK_TMACHINE");
        });

        modelBuilder.Entity<TPCBANGMASTERTABLE>(entity =>
        {
            entity.HasKey(e => e.dwPcBangID).HasFillFactor(80);

            entity.ToTable("TPCBANGMASTERTABLE");

            entity.HasIndex(e => e.dWorld1, "IX_TPCBANGMASTERTABLE").HasFillFactor(80);

            entity.HasIndex(e => e.dWorld2, "IX_TPCBANGMASTERTABLE_1").HasFillFactor(80);

            entity.HasIndex(e => e.dWorld3, "IX_TPCBANGMASTERTABLE_2").HasFillFactor(80);

            entity.HasIndex(e => e.dWorld4, "IX_TPCBANGMASTERTABLE_3").HasFillFactor(80);

            entity.Property(e => e.dwPcBangID).ValueGeneratedNever();
            entity.Property(e => e.dWorld1).HasColumnType("smalldatetime");
            entity.Property(e => e.dWorld2).HasColumnType("smalldatetime");
            entity.Property(e => e.dWorld3).HasColumnType("smalldatetime");
            entity.Property(e => e.dWorld4).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TPCBANGPLAYTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.dwPlayDate }).HasFillFactor(80);

            entity.ToTable("TPCBANGPLAYTABLE");
        });

        modelBuilder.Entity<TPREVERSION>(entity =>
        {
            entity.HasKey(e => e.dwBetaVer)
                .HasName("PK__TPREVERSION__24134F1B")
                .HasFillFactor(80);

            entity.ToTable("TPREVERSION");

            entity.Property(e => e.dwBetaVer).ValueGeneratedNever();
            entity.Property(e => e.szName)
                .HasMaxLength(260)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szPath)
                .HasMaxLength(260)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TREGIONCHART>(entity =>
        {
            entity.HasKey(e => e.dwID).HasFillFactor(80);

            entity.ToTable("TREGIONCHART");

            entity.Property(e => e.dwID).ValueGeneratedNever();
            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TRESERVEDNAME>(entity =>
        {
            entity.HasKey(e => e.szName).HasFillFactor(80);

            entity.ToTable("TRESERVEDNAME");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.dwSeq).ValueGeneratedOnAdd();
        });

        modelBuilder.Entity<TSECURECODE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSECURECODE");

            entity.Property(e => e.strSecurityCode)
                .HasMaxLength(55)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TSERVER>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSERVER");

            entity.HasIndex(e => e.bType, "IX_TSERVER_TYPE").HasFillFactor(80);

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .HasDefaultValue("");

            entity.HasOne(d => d.bGroup).WithMany()
                .HasForeignKey(d => d.bGroupID)
                .OnDelete(DeleteBehavior.ClientSetNull)
                .HasConstraintName("FK__TSERVER__bGroupI__009508B4");

            entity.HasOne(d => d.bMachine).WithMany()
                .HasForeignKey(d => d.bMachineID)
                .OnDelete(DeleteBehavior.ClientSetNull)
                .HasConstraintName("FK__TSERVER__bMachin__01892CED");
        });

        modelBuilder.Entity<TSMSTABLE>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TSMSTABLE");

            entity.HasIndex(e => e.dwUserID, "IX_TSMSTABLE").HasFillFactor(80);

            entity.Property(e => e.bResult)
                .HasMaxLength(1)
                .IsUnicode(false)
                .IsFixedLength()
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.dateSend)
                .HasDefaultValueSql("(getdate())", "DF_TSMSTABLE_dateSend")
                .HasColumnType("smalldatetime");
            entity.Property(e => e.dwSeq).ValueGeneratedOnAdd();
            entity.Property(e => e.szCharName)
                .HasMaxLength(20)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szMessage)
                .HasMaxLength(255)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szSender)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TSVRTYPE>(entity =>
        {
            entity.HasKey(e => e.bType).HasFillFactor(80);

            entity.ToTable("TSVRTYPE");

            entity.Property(e => e.szName)
                .HasMaxLength(50)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TTEMPCASHITEM>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TTEMPCASHITEM");
        });

        modelBuilder.Entity<TTEMPDURINGITEMTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.wItemID }).HasFillFactor(80);

            entity.ToTable("TTEMPDURINGITEMTABLE");

            entity.Property(e => e.dEndTime).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TTESTLOGINUSER>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TTESTLOGINUSER");
        });

        modelBuilder.Entity<TUNIFYCASHITEM>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TUNIFYCASHITEM");

            entity.HasIndex(e => e.dwUserID, "IX_TUNIFYCASHITEM").HasFillFactor(80);
        });

        modelBuilder.Entity<TUNIFYPOSTITEMTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.dwPostID }).HasFillFactor(80);

            entity.ToTable("TUNIFYPOSTITEMTABLE");

            entity.Property(e => e.dwPostID).ValueGeneratedOnAdd();
        });

        modelBuilder.Entity<TUSERATTENDTABLE>(entity =>
        {
            entity.HasKey(e => new { e.dwUserID, e.bDay }).HasFillFactor(80);

            entity.ToTable("TUSERATTENDTABLE");
        });

        modelBuilder.Entity<TUSERINFOTABLE>(entity =>
        {
            entity.HasKey(e => e.dwUserID).HasFillFactor(80);

            entity.ToTable("TUSERINFOTABLE");

            entity.Property(e => e.dwUserID).ValueGeneratedNever();
            entity.Property(e => e.bCanCreateCharCount).HasDefaultValue((byte)6, "DF_TUSERINFOTABLE_bCanCreateCharCount");
            entity.Property(e => e.dCabinetUse)
                .HasDefaultValue(new DateTime(2008, 1, 1, 0, 0, 0, 0, DateTimeKind.Unspecified), "DF_TUSERINFOTABLE_dCabinetUse")
                .HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<TUSERPROTECTED>(entity =>
        {
            entity.HasKey(e => e.dwSeq)
                .HasName("PK__TUSERPRO__E15A5BA13BEAD8AC")
                .HasFillFactor(80);

            entity.ToTable("TUSERPROTECTED");

            entity.HasIndex(e => e.dwUserID, "IX_TUSERPROTECTED_USERID").HasFillFactor(80);

            entity.Property(e => e.regDate)
                .HasDefaultValueSql("(getdate())", "DF_TUSERPROTECTED_regDate")
                .HasColumnType("datetime");
            entity.Property(e => e.startTime).HasColumnType("datetime");
            entity.Property(e => e.szCharName)
                .HasMaxLength(20)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szComment)
                .HasMaxLength(4000)
                .UseCollation("Korean_Wansung_CI_AS");
            entity.Property(e => e.szGMID)
                .HasMaxLength(20)
                .IsUnicode(false)
                .UseCollation("Korean_Wansung_CI_AS");
        });

        modelBuilder.Entity<TUSERPROTECTED_old>(entity =>
        {
            entity.HasKey(e => e.dwSeq)
                .HasName("PK__TUSERPRO__E15A5BA150B0EB68")
                .HasFillFactor(80);

            entity.ToTable("TUSERPROTECTED_old");

            entity.HasIndex(e => e.dwUserID, "IX_TUSERPROTECTED_USERID").HasFillFactor(80);

            entity.Property(e => e.regDate)
                .HasDefaultValueSql("(getdate())")
                .HasColumnType("datetime");
            entity.Property(e => e.startTime).HasColumnType("datetime");
            entity.Property(e => e.szCharName)
                .HasMaxLength(20)
                .IsUnicode(false);
            entity.Property(e => e.szComment).HasMaxLength(4000);
            entity.Property(e => e.szGMID)
                .HasMaxLength(20)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TUSER_INTERFACE>(entity =>
        {
            entity.HasKey(e => e.bOption).HasName("PK__TUSER_IN__264479056A70BD6B");

            entity.ToTable("TUSER_INTERFACE");

            entity.Property(e => e.szName)
                .HasMaxLength(260)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVERSION>(entity =>
        {
            entity.HasKey(e => e.dwVersion)
                .HasName("PK__TVERSION__895A315451DA19CB")
                .HasFillFactor(80);

            entity.ToTable("TVERSION");

            entity.HasIndex(e => new { e.szPath, e.szName }, "UQ__TVERSION__A59CAA5454B68676")
                .IsUnique()
                .HasFillFactor(80);

            entity.Property(e => e.dwVersion).ValueGeneratedNever();
            entity.Property(e => e.szName)
                .HasMaxLength(260)
                .IsUnicode(false);
            entity.Property(e => e.szPath)
                .HasMaxLength(260)
                .IsUnicode(false);
        });

        modelBuilder.Entity<TVETERANCHART>(entity =>
        {
            entity.HasKey(e => e.bID)
                .HasName("PK__TVETERAN__DE99891F168449D3")
                .HasFillFactor(80);

            entity.ToTable("TVETERANCHART");
        });

        modelBuilder.Entity<TempEvent>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("TempEvent");

            entity.Property(e => e.CheckDate).HasColumnType("smalldatetime");
            entity.Property(e => e.timeEnd).HasColumnType("datetime");
            entity.Property(e => e.timeStart).HasColumnType("datetime");
        });

        modelBuilder.Entity<USERIPLOG>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("USERIPLOG");

            entity.Property(e => e.Date_time).HasColumnType("smalldatetime");
            entity.Property(e => e.IP).IsUnicode(false);
            entity.Property(e => e.Username).IsUnicode(false);
        });

        modelBuilder.Entity<releaseDate>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("releaseDate");

            entity.Property(e => e.dReleaseDate).HasColumnType("smalldatetime");
        });

        modelBuilder.Entity<ttemplevel>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("ttemplevel");
        });

        modelBuilder.Entity<ttempuser>(entity =>
        {
            entity
                .HasNoKey()
                .ToTable("ttempuser");
        });

        OnModelCreatingPartial(modelBuilder);
    }

    partial void OnModelCreatingPartial(ModelBuilder modelBuilder);
}
