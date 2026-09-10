{
  GRTest-Builder.pas  —  xEdit (SSEEdit/TES5Edit) script
  Builds the GlobalRules test plugin: GRTest.esp

  Creates:
    - 19 float GLOB records (GRT_*)
    - 2 PERK records used as condition containers:
        GRT_P_AlwaysTrue      (no conditions -> always true)
        GRT_P_InThievesGuild  (Subject GetInFaction <Thieves Guild> == 1)

  How to use:
    1. Copy this file into xEdit's "Edit Scripts" folder.
    2. Launch xEdit and load Skyrim.esm.
    3. In the left tree, right-click any plugin -> Apply Script...
       -> GRTest-Builder -> OK.
    4. The new plugin is created IN MEMORY. Save it with Ctrl+S
       (or right-click GRTest.esp -> Save). Enable it in your load order.

  Tested against xEdit dev-4.1.6 scripting API.
}

unit GRTestBuilder;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

function AddFloatGlobal(aGroup: IInterface; const aEditorID: string): IInterface;
var
  r: IInterface;
begin
  r := Add(aGroup, 'GLOB', True);
  SetElementEditValues(r, 'EDID', aEditorID);
  SetElementEditValues(r, 'FNAM', 'Float');   // 'Float' | 'Long' | 'Short'
  SetElementNativeValues(r, 'FLTV', 0.0);
  Result := r;
end;

function FindFactionByEditorID(const aEditorID: string): IInterface;
var
  factGrp: IInterface;
begin
  Result := nil;
  factGrp := GroupBySignature(FileByIndex(0), 'FACT');
  if not Assigned(factGrp) then
    Exit;
  Result := MainRecordByEditorID(factGrp, aEditorID);
  if not Assigned(Result) then
    Result := MainRecordByEditorID(factGrp, 'ThievesGuild');
  if not Assigned(Result) then
    Result := MainRecordByEditorID(factGrp, 'GuildThievesGuild');
end;

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

function Initialize: Integer;
var
  f, globGrp, perkGrp, perk, conditions, cond, ctda, faction: IInterface;
begin
  Result := 0;

  f := AddNewFileName('GRTest.esp');
  if not Assigned(f) then begin
    AddMessage('GRTest.esp creation cancelled.');
    Result := 1;
    Exit;
  end;

  AddMasterIfMissing(f, 'Skyrim.esm');

  // ----- Globals (all Float, value 0) --------------------------------------
  globGrp := GroupBySignature(f, 'GLOB');
  if not Assigned(globGrp) then
    globGrp := Add(f, 'GLOB', True);

  AddFloatGlobal(globGrp, 'GRT_Debug');
  AddFloatGlobal(globGrp, 'GRT_ActivateCount');
  AddFloatGlobal(globGrp, 'GRT_EquipState');
  AddFloatGlobal(globGrp, 'GRT_KillCount');
  AddFloatGlobal(globGrp, 'GRT_MenuOpen');
  AddFloatGlobal(globGrp, 'GRT_ItemDelta');
  AddFloatGlobal(globGrp, 'GRT_QuestStage');
  AddFloatGlobal(globGrp, 'GRT_CellEnter');
  AddFloatGlobal(globGrp, 'GRT_LevelReward');
  AddFloatGlobal(globGrp, 'GRT_Expr');
  AddFloatGlobal(globGrp, 'GRT_Constant');
  AddFloatGlobal(globGrp, 'GRT_Clamp');
  AddFloatGlobal(globGrp, 'GRT_Stat');
  AddFloatGlobal(globGrp, 'GRT_FormID');
  AddFloatGlobal(globGrp, 'GRT_PerkPass');
  AddFloatGlobal(globGrp, 'GRT_PerkInvert');
  AddFloatGlobal(globGrp, 'GRT_LastWins');
  AddFloatGlobal(globGrp, 'GRT_NonFinite');
  AddFloatGlobal(globGrp, 'GRT_BadExpr');

  AddMessage('Created 19 globals.');

  // ----- Perks (condition containers) --------------------------------------
  perkGrp := GroupBySignature(f, 'PERK');
  if not Assigned(perkGrp) then
    perkGrp := Add(f, 'PERK', True);

  // GRT_P_AlwaysTrue : no conditions (empty list evaluates true)
  perk := Add(perkGrp, 'PERK', True);
  SetElementEditValues(perk, 'EDID', 'GRT_P_AlwaysTrue');
  SetElementNativeValues(perk, 'DATA\Trait', 0);
  SetElementNativeValues(perk, 'DATA\Level', 0);
  SetElementNativeValues(perk, 'DATA\Num Ranks', 1);
  SetElementNativeValues(perk, 'DATA\Playable', 1);
  SetElementNativeValues(perk, 'DATA\Hidden', 0);

  // GRT_P_InThievesGuild : Subject GetInFaction <ThievesGuild> == 1
  perk := Add(perkGrp, 'PERK', True);
  SetElementEditValues(perk, 'EDID', 'GRT_P_InThievesGuild');
  SetElementNativeValues(perk, 'DATA\Trait', 0);
  SetElementNativeValues(perk, 'DATA\Level', 0);
  SetElementNativeValues(perk, 'DATA\Num Ranks', 1);
  SetElementNativeValues(perk, 'DATA\Playable', 1);
  SetElementNativeValues(perk, 'DATA\Hidden', 0);

  faction := FindFactionByEditorID('ThievesGuildFaction');
  if Assigned(faction) then begin
    conditions := Add(perk, 'Conditions', True);
    cond := Add(conditions, 'Condition', True);
    ctda := ElementBySignature(cond, 'CTDA');

    SetElementEditValues (ctda, 'Type', '10000000');                            // ==
    SetElementNativeValues(ctda, 'Comparison Value', 1.0);                      // == 1
    SetElementEditValues (ctda, 'Function', 'GetInFaction');
    SetElementNativeValues(ctda, 'Parameter #1', GetLoadOrderFormID(faction));  // Thieves Guild
    SetElementEditValues (ctda, 'Run On', 'Subject');

    AddMessage('GRT_P_InThievesGuild condition -> ' + Name(faction));
  end else
    AddMessage('WARNING: Thieves Guild faction not found; GRT_P_InThievesGuild left without a condition.');

  AddMessage('GRTest.esp built in memory. Save it with Ctrl+S.');
end;

function Process(e: IInterface): Integer;
begin
  Result := 0;
end;

function Finalize: Integer;
begin
  Result := 0;
end;

end.
