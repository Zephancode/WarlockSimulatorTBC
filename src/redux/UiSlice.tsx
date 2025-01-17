import { createSlice, PayloadAction } from "@reduxjs/toolkit";
import {
  CombatLogBreakdown,
  GemSelectionTableStruct,
  InitialGemSelectionTableValue,
  ItemSlot,
  ItemSlotKey,
  Phase,
  Stat,
  StatWeightStats,
  SubSlotValue,
  UiState,
} from "../Types";

const initialUiState: UiState = {
  sources: JSON.parse(
    localStorage.getItem("sources") ||
      JSON.stringify({
        phase: { 0: true, 1: true, 2: true, 3: true, 4: true, 5: true },
      })
  ),
  gemSelectionTable: InitialGemSelectionTableValue,
  gemPreferences: JSON.parse(
    localStorage.getItem("gemPreferences") ||
      JSON.stringify({ hidden: [], favorites: [] })
  ),
  selectedProfile: localStorage.getItem("selectedProfile") || "",
  importExportWindowVisible: false,
  equippedItemsWindowVisible: false,
  fillItemSocketsWindowVisible: false,
  hiddenItems: JSON.parse(
    localStorage.getItem("hiddenItems") || JSON.stringify([])
  ),
  selectedItemSlot:
    (localStorage.getItem("selectedItemSlot") as ItemSlotKey) ||
    ItemSlotKey.Mainhand,
  selectedItemSubSlot:
    (localStorage.getItem("selectedItemSubSlot") as SubSlotValue) || "1",
  savedItemDps: JSON.parse(localStorage.getItem("savedItemDps") || "{}"),
  combatLog: { visible: false, data: [] },
  combatLogBreakdown: {
    totalDamageDone: 0,
    totalManaGained: 0,
    totalSimulationFightLength: 0,
    totalIterationAmount: 0,
    spellDamageDict: {},
    spellManaGainDict: {},
    data: [],
  },
  histogram: { visible: false },
  simulationInProgress: false,
  statWeights: {
    visible: false,
    statValues: {
      [Stat.stamina]: 0,
      [Stat.intellect]: 0,
      [Stat.spirit]: 0,
      [Stat.spellPower]: 0,
      [Stat.shadowPower]: 0,
      [Stat.firePower]: 0,
      [Stat.hitRating]: 0,
      [Stat.critRating]: 0,
      [Stat.hasteRating]: 0,
      [Stat.mp5]: 0,
    },
  },
};

export const UiSlice = createSlice({
  name: "ui",
  initialState: initialUiState,
  reducers: {
    togglePhase: (state, action: PayloadAction<Phase>) => {
      state.sources.phase[action.payload] =
        !state.sources.phase[action.payload];
      localStorage.setItem("sources", JSON.stringify(state.sources));
    },
    setGemSelectionTable: (
      state,
      action: PayloadAction<GemSelectionTableStruct>
    ) => {
      state.gemSelectionTable = action.payload;
    },
    favoriteGem: (state, action: PayloadAction<number>) => {
      if (state.gemPreferences.favorites.includes(action.payload)) {
        state.gemPreferences.favorites = state.gemPreferences.favorites.filter(
          (e) => e !== action.payload
        );
      } else {
        state.gemPreferences.favorites.push(action.payload);
      }

      localStorage.setItem(
        "gemPreferences",
        JSON.stringify(state.gemPreferences)
      );
    },
    hideGem: (state, action: PayloadAction<number>) => {
      if (state.gemPreferences.hidden.includes(action.payload)) {
        state.gemPreferences.hidden = state.gemPreferences.hidden.filter(
          (e) => e !== action.payload
        );
      } else {
        state.gemPreferences.hidden.push(action.payload);
      }

      localStorage.setItem(
        "gemPreferences",
        JSON.stringify(state.gemPreferences)
      );
    },
    setSelectedProfile: (state, action: PayloadAction<string>) => {
      state.selectedProfile = action.payload;
      localStorage.setItem("selectedProfile", action.payload);
    },
    setImportExportWindowVisibility: (
      state,
      action: PayloadAction<boolean>
    ) => {
      state.importExportWindowVisible = action.payload;
    },
    setEquippedItemsWindowVisibility: (
      state,
      action: PayloadAction<boolean>
    ) => {
      state.equippedItemsWindowVisible = action.payload;
    },
    setFillItemSocketsWindowVisibility: (
      state,
      action: PayloadAction<boolean>
    ) => {
      state.fillItemSocketsWindowVisible = action.payload;
    },
    toggleHiddenItemId: (state, action: PayloadAction<number>) => {
      if (state.hiddenItems.includes(action.payload)) {
        state.hiddenItems = state.hiddenItems.filter(
          (e) => e !== action.payload
        );
      } else {
        state.hiddenItems.push(action.payload);
      }

      localStorage.setItem("hiddenItems", JSON.stringify(state.hiddenItems));
    },
    setSelectedItemSlot: (state, action: PayloadAction<ItemSlotKey>) => {
      state.selectedItemSlot = action.payload;
      localStorage.setItem("selectedItemSlot", state.selectedItemSlot);
    },
    setSelectedItemSubSlot: (state, action: PayloadAction<SubSlotValue>) => {
      state.selectedItemSubSlot = action.payload;
      localStorage.setItem("selectedItemSubSlot", state.selectedItemSubSlot);
    },
    setSavedItemDps: (
      state,
      action: PayloadAction<{
        itemSlot: ItemSlot;
        itemId: number;
        dps: number;
        saveLocalStorage: boolean;
      }>
    ) => {
      if (!state.savedItemDps[action.payload.itemSlot]) {
        state.savedItemDps[action.payload.itemSlot] = {};
      }

      state.savedItemDps[action.payload.itemSlot][action.payload.itemId] =
        action.payload.dps;
      if (action.payload.saveLocalStorage) {
        localStorage.setItem(
          "savedItemDps",
          JSON.stringify(state.savedItemDps)
        );
      }
    },
    setCombatLogVisibility: (state, action: PayloadAction<boolean>) => {
      state.combatLog.visible = action.payload;
    },
    setCombatLogData: (state, action: PayloadAction<string[]>) => {
      state.combatLog.data = action.payload;
    },
    setCombatLogBreakdownValue: (
      state,
      action: PayloadAction<CombatLogBreakdown>
    ) => {
      state.combatLogBreakdown = action.payload;
    },
    clearSavedItemSlotDps: (state, action: PayloadAction<ItemSlot>) => {
      state.savedItemDps[action.payload] = {};
    },
    setHistogramVisibility: (state, action: PayloadAction<boolean>) => {
      state.histogram.visible = action.payload;
    },
    setHistogramData: (
      state,
      action: PayloadAction<{ [key: string]: number }>
    ) => {
      state.histogram.data = action.payload;
    },
    setStatWeightVisibility: (state, action: PayloadAction<boolean>) => {
      state.statWeights.visible = action.payload;
    },
    setStatWeightValue: (
      state,
      action: PayloadAction<{ stat: [keyof StatWeightStats]; value: number }>
    ) => {
      state.statWeights.statValues[action.payload.stat as unknown as Stat] =
        action.payload.value;
    },
    setSimulationInProgressStatus: (state, action: PayloadAction<boolean>) => {
      state.simulationInProgress = action.payload;
    },
  },
});

export const {
  setStatWeightValue,
  setSimulationInProgressStatus,
  setStatWeightVisibility,
  setHistogramData,
  setHistogramVisibility,
  clearSavedItemSlotDps,
  setCombatLogBreakdownValue,
  setCombatLogData,
  setCombatLogVisibility,
  setSavedItemDps,
  setSelectedItemSubSlot,
  setSelectedItemSlot,
  setFillItemSocketsWindowVisibility,
  setEquippedItemsWindowVisibility,
  toggleHiddenItemId,
  setImportExportWindowVisibility,
  setSelectedProfile,
  togglePhase,
  setGemSelectionTable,
  favoriteGem,
  hideGem,
} = UiSlice.actions;
export default UiSlice.reducer;
