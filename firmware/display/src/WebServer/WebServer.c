#include "WebServer.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs.h"
#include "cJSON.h"
#include "BrewProfileStore.h"

static const char *TAG = "WebServer";

static httpd_handle_t s_server = NULL;
static bool s_initialized = false;

static bool is_storage_full_error(esp_err_t err)
{
    if (err == ESP_ERR_NO_MEM || err == ESP_ERR_NVS_NOT_ENOUGH_SPACE)
    {
        return true;
    }
#ifdef ESP_ERR_NVS_PART_NOT_ENOUGH_SPACE
    if (err == ESP_ERR_NVS_PART_NOT_ENOUGH_SPACE)
    {
        return true;
    }
#endif
#ifdef ESP_ERR_NVS_FULL
    if (err == ESP_ERR_NVS_FULL)
    {
        return true;
    }
#endif
    return false;
}

static const char INDEX_HTML[] =
    "<!DOCTYPE html>\n"
    "<html lang=\"en\">\n"
    "<head>\n"
    "<meta charset=\"utf-8\">\n"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n"
    "<title>Gagguino Brew Profiles</title>\n"
    "<style>\n"
    "body{font-family:Arial,sans-serif;margin:20px;background:#f4f4f4;color:#333;}\n"
    "h1{margin-bottom:16px;}\n"
    "#messages{margin-bottom:16px;min-height:1.2em;}\n"
    ".card{background:#fff;padding:16px;margin-bottom:16px;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}\n"
    "label{display:block;margin-top:8px;font-weight:bold;}\n"
    "label input,label textarea{display:block;margin-top:4px;}\n"
    "input[type=\"text\"],textarea{width:100%;padding:8px;box-sizing:border-box;border:1px solid #ccc;border-radius:4px;}\n"
    "textarea{min-height:180px;font-family:monospace;}\n"
    "button{margin-top:12px;padding:8px 16px;border:none;border-radius:4px;background:#1976d2;color:#fff;cursor:pointer;}\n"
    "button:hover{background:#125a9c;}\n"
    "button:disabled{background:#9e9e9e;cursor:default;}\n"
    "button.secondary{background:#e0e0e0;color:#333;}\n"
    "button.secondary:hover{background:#c2c2c2;}\n"
    ".hidden{display:none;}\n"
    ".profile-row{display:flex;justify-content:space-between;align-items:center;padding:12px;border:1px solid #ddd;border-radius:6px;margin-top:12px;background:#fafafa;}\n"
    ".profile-row:first-child{margin-top:0;}\n"
    ".profile-row-info{flex:1;min-width:0;}\n"
    ".profile-row-info h3{margin:0 0 4px 0;font-size:1rem;}\n"
    ".profile-row-info p{margin:0;color:#555;font-size:0.9rem;}\n"
    ".profile-row-actions{display:flex;gap:8px;flex-wrap:wrap;justify-content:flex-end;}\n"
    ".actions{display:flex;gap:8px;margin-top:12px;}\n"
    ".list-header{display:flex;justify-content:space-between;align-items:center;gap:12px;}\n"
    ".phase-editor{margin-top:16px;}\n"
    ".phase-editor-header{display:flex;align-items:center;justify-content:space-between;gap:12px;margin-bottom:8px;}\n"
    ".phase-editor-header h3{margin:0;font-size:1rem;}\n"
    ".phase-list{display:flex;flex-direction:column;gap:12px;}\n"
    ".phase-item{border:1px solid #ddd;border-radius:6px;padding:12px;background:#fff;}\n"
    ".phase-header{display:flex;justify-content:space-between;align-items:center;gap:8px;margin-bottom:12px;}\n"
    ".phase-title{margin:0;font-size:1rem;}\n"
    ".phase-controls{display:flex;gap:8px;flex-wrap:wrap;}\n"
    ".phase-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;}\n"
    ".phase-grid label{margin-top:0;}\n"
    ".phase-empty{color:#777;font-style:italic;margin:8px 0;}\n"
    "button.small{padding:4px 10px;font-size:0.85rem;}\n"
    ".success{color:#2e7d32;}\n"
    ".error{color:#b00020;}\n"
    "</style>\n"
    "</head>\n"
    "<body>\n"
    "<h1>Brew Profiles</h1>\n"
    "<div id=\"messages\"></div>\n"
    "<div class=\"card\">\n"
    "  <h2>Active Profile</h2>\n"
    "  <p id=\"active-profile\">None selected</p>\n"
    "</div>\n"
    "<div class=\"card\">\n"
    "  <div class=\"list-header\">\n"
    "    <h2>Available Profiles</h2>\n"
    "    <button id=\"add-profile-button\" class=\"secondary\" type=\"button\">Add Profile</button>\n"
    "  </div>\n"
    "  <div id=\"profile-list\"></div>\n"
    "</div>\n"
    "<div class=\"card hidden\" id=\"editor-card\">\n"
    "  <h2 id=\"editor-title\">Edit Profile</h2>\n"
    "  <form id=\"editor-form\">\n"
    "    <label>Name<input name=\"name\" type=\"text\" required></label>\n"
    "    <div class=\"phase-editor\">\n"
    "      <div class=\"phase-editor-header\">\n"
    "        <h3>Phases</h3>\n"
    "        <button type=\"button\" id=\"add-phase\" class=\"secondary\">Add Phase</button>\n"
    "      </div>\n"
    "      <div id=\"phase-list\" class=\"phase-list\"></div>\n"
    "      <p id=\"phase-empty\" class=\"phase-empty hidden\">No phases yet. Add one to get started.</p>\n"
    "    </div>\n"
    "    <div class=\"actions\">\n"
    "      <button type=\"submit\">Save</button>\n"
    "      <button type=\"button\" id=\"cancel-edit\" class=\"secondary\">Cancel</button>\n"
    "    </div>\n"
    "  </form>\n"
    "</div>\n"
    "<script>\n"
    "const messages = document.getElementById('messages');\n"
    "const activeProfileEl = document.getElementById('active-profile');\n"
    "const profileList = document.getElementById('profile-list');\n"
    "const editorCard = document.getElementById('editor-card');\n"
    "const editorTitle = document.getElementById('editor-title');\n"
    "const editorForm = document.getElementById('editor-form');\n"
    "const cancelEditBtn = document.getElementById('cancel-edit');\n"
    "const addProfileButton = document.getElementById('add-profile-button');\n"
    "const phaseList = document.getElementById('phase-list');\n"
    "const phaseEmpty = document.getElementById('phase-empty');\n"
    "const addPhaseButton = document.getElementById('add-phase');\n"
    "const state = { profiles: [], activeIndex: null, editingIndex: null, editingPhases: [] };\n"
    "const durationModes = ['time', 'volume', 'mass'];\n"
    "const durationLabels = { time: 'Time (s)', volume: 'Volume (ml)', mass: 'Mass (g)' };\n"
    "const pumpModes = ['power', 'pressure'];\n"
    "const pumpLabels = { power: 'Pump Power (%)', pressure: 'Pump Pressure (bar)' };\n"
    "const defaultPhaseValues = { durationMode: 'time', durationValue: 30, pumpMode: 'power', pumpValue: 95, temperatureC: 92 };\n"
    "const MAX_PHASES = 12;\n"
    "\n"
    "function sanitizeNumber(value, fallback) {\n"
    "  const num = Number(value);\n"
    "  return Number.isFinite(num) ? num : fallback;\n"
    "}\n"
    "\n"
    "function normalizeMode(value, allowed, fallback) {\n"
    "  return allowed.includes(value) ? value : fallback;\n"
    "}\n"
    "\n"
    "function clonePhase(phase, index) {\n"
    "  const safe = phase || {};\n"
    "  const fallbackName = `Phase ${index + 1}`;\n"
    "  const name = typeof safe.name === 'string' && safe.name.trim().length ? safe.name : fallbackName;\n"
    "  return {\n"
    "    name,\n"
    "    durationMode: normalizeMode(safe.durationMode, durationModes, defaultPhaseValues.durationMode),\n"
    "    durationValue: sanitizeNumber(safe.durationValue, defaultPhaseValues.durationValue),\n"
    "    pumpMode: normalizeMode(safe.pumpMode, pumpModes, defaultPhaseValues.pumpMode),\n"
    "    pumpValue: sanitizeNumber(safe.pumpValue, defaultPhaseValues.pumpValue),\n"
    "    temperatureC: sanitizeNumber(safe.temperatureC, defaultPhaseValues.temperatureC)\n"
    "  };\n"
    "}\n"
    "\n"
    "function createNewPhase(index) {\n"
    "  return clonePhase({\n"
    "    name: `Phase ${index + 1}`,\n"
    "    durationMode: defaultPhaseValues.durationMode,\n"
    "    durationValue: defaultPhaseValues.durationValue,\n"
    "    pumpMode: defaultPhaseValues.pumpMode,\n"
    "    pumpValue: defaultPhaseValues.pumpValue,\n"
    "    temperatureC: defaultPhaseValues.temperatureC\n"
    "  }, index);\n"
    "}\n"
    "\n"
    "function showMessage(text, isError = false) {\n"
    "  messages.textContent = text;\n"
    "  messages.className = isError ? 'error' : 'success';\n"
    "  if (text) {\n"
    "    setTimeout(() => {\n"
    "      messages.textContent = '';\n"
    "      messages.className = '';\n"
    "    }, 5000);\n"
    "  }\n"
    "}\n"
    "\n"
    "function setActiveDisplay() {\n"
    "  if (state.activeIndex === null || state.activeIndex < 0 || state.activeIndex >= state.profiles.length) {\n"
    "    activeProfileEl.textContent = 'None selected';\n"
    "    return;\n"
    "  }\n"
    "  const profile = state.profiles[state.activeIndex];\n"
    "  activeProfileEl.textContent = `${profile.name} (Profile ${state.activeIndex + 1})`;\n"
    "}\n"
    "\n"
    "function createActionButton(label, handler, options = {}) {\n"
    "  const btn = document.createElement('button');\n"
    "  btn.type = 'button';\n"
    "  btn.textContent = label;\n"
    "  if (options.secondary) btn.classList.add('secondary');\n"
    "  if (options.disabled) {\n"
    "    btn.disabled = true;\n"
    "  }\n"
    "  btn.addEventListener('click', handler);\n"
    "  return btn;\n"
    "}\n"
    "\n"
    "function movePhase(index, delta) {\n"
    "  const newIndex = index + delta;\n"
    "  if (newIndex < 0 || newIndex >= state.editingPhases.length) {\n"
    "    return;\n"
    "  }\n"
    "  const phases = state.editingPhases;\n"
    "  const [phase] = phases.splice(index, 1);\n"
    "  phases.splice(newIndex, 0, phase);\n"
    "  renderPhaseList();\n"
    "}\n"
    "\n"
    "function removePhase(index) {\n"
    "  state.editingPhases.splice(index, 1);\n"
    "  renderPhaseList();\n"
    "}\n"
    "\n"
    "function renderPhaseList() {\n"
    "  phaseList.innerHTML = '';\n"
    "  if (state.editingPhases.length === 0) {\n"
    "    phaseEmpty.classList.remove('hidden');\n"
    "    addPhaseButton.disabled = false;\n"
    "    return;\n"
    "  }\n"
    "  phaseEmpty.classList.add('hidden');\n"
    "  state.editingPhases.forEach((phase, index) => {\n"
    "    const container = document.createElement('div');\n"
    "    container.className = 'phase-item';\n"
    "\n"
    "    const header = document.createElement('div');\n"
    "    header.className = 'phase-header';\n"
    "\n"
    "    const title = document.createElement('h3');\n"
    "    title.className = 'phase-title';\n"
    "    header.appendChild(title);\n"
    "\n"
    "    const controls = document.createElement('div');\n"
    "    controls.className = 'phase-controls';\n"
    "\n"
    "    const upBtn = createActionButton('Up', () => movePhase(index, -1), { secondary: true });\n"
    "    upBtn.classList.add('small');\n"
    "    upBtn.disabled = index === 0;\n"
    "    controls.appendChild(upBtn);\n"
    "\n"
    "    const downBtn = createActionButton('Down', () => movePhase(index, 1), { secondary: true });\n"
    "    downBtn.classList.add('small');\n"
    "    downBtn.disabled = index === state.editingPhases.length - 1;\n"
    "    controls.appendChild(downBtn);\n"
    "\n"
    "    const deleteBtn = createActionButton('Delete', () => removePhase(index), { secondary: true });\n"
    "    deleteBtn.classList.add('small');\n"
    "    controls.appendChild(deleteBtn);\n"
    "\n"
    "    header.appendChild(controls);\n"
    "    container.appendChild(header);\n"
    "\n"
    "    const grid = document.createElement('div');\n"
    "    grid.className = 'phase-grid';\n"
    "\n"
    "    const nameLabel = document.createElement('label');\n"
    "    nameLabel.textContent = 'Name';\n"
    "    const nameInput = document.createElement('input');\n"
    "    nameInput.type = 'text';\n"
    "    nameInput.value = phase.name || '';\n"
    "    nameInput.required = true;\n"
    "    const updateTitle = () => {\n"
    "      const trimmed = nameInput.value.trim();\n"
    "      title.textContent = trimmed ? `${index + 1}. ${trimmed}` : `Phase ${index + 1}`;\n"
    "    };\n"
    "    updateTitle();\n"
    "    nameInput.addEventListener('input', (event) => {\n"
    "      state.editingPhases[index].name = event.target.value;\n"
    "      updateTitle();\n"
    "    });\n"
    "    nameLabel.appendChild(nameInput);\n"
    "    grid.appendChild(nameLabel);\n"
    "\n"
    "    const durationModeLabel = document.createElement('label');\n"
    "    durationModeLabel.textContent = 'Duration Mode';\n"
    "    const durationModeSelect = document.createElement('select');\n"
    "    durationModes.forEach((mode) => {\n"
    "      const option = document.createElement('option');\n"
    "      option.value = mode;\n"
    "      option.textContent = durationLabels[mode] || mode;\n"
    "      durationModeSelect.appendChild(option);\n"
    "    });\n"
    "    durationModeSelect.value = phase.durationMode;\n"
    "    durationModeSelect.addEventListener('change', (event) => {\n"
    "      const value = event.target.value;\n"
    "      state.editingPhases[index].durationMode = value;\n"
    "      durationValueInput.placeholder = durationLabels[value] || 'Value';\n"
    "    });\n"
    "    durationModeLabel.appendChild(durationModeSelect);\n"
    "    grid.appendChild(durationModeLabel);\n"
    "\n"
    "    const durationValueLabel = document.createElement('label');\n"
    "    durationValueLabel.textContent = 'Duration Value';\n"
    "    const durationValueInput = document.createElement('input');\n"
    "    durationValueInput.type = 'number';\n"
    "    durationValueInput.min = '0';\n"
    "    durationValueInput.step = 'any';\n"
    "    durationValueInput.value = phase.durationValue === '' ? '' : phase.durationValue;\n"
    "    durationValueInput.placeholder = durationLabels[phase.durationMode] || 'Value';\n"
    "    durationValueInput.addEventListener('input', (event) => {\n"
    "      const value = event.target.value;\n"
    "      state.editingPhases[index].durationValue = value === '' ? '' : Number(value);\n"
    "    });\n"
    "    durationValueLabel.appendChild(durationValueInput);\n"
    "    grid.appendChild(durationValueLabel);\n"
    "\n"
    "    const pumpModeLabel = document.createElement('label');\n"
    "    pumpModeLabel.textContent = 'Pump Mode';\n"
    "    const pumpModeSelect = document.createElement('select');\n"
    "    pumpModes.forEach((mode) => {\n"
    "      const option = document.createElement('option');\n"
    "      option.value = mode;\n"
    "      option.textContent = pumpLabels[mode] || mode;\n"
    "      pumpModeSelect.appendChild(option);\n"
    "    });\n"
    "    pumpModeSelect.value = phase.pumpMode;\n"
    "    pumpModeSelect.addEventListener('change', (event) => {\n"
    "      const value = event.target.value;\n"
    "      state.editingPhases[index].pumpMode = value;\n"
    "      pumpValueInput.placeholder = pumpLabels[value] || 'Value';\n"
    "    });\n"
    "    pumpModeLabel.appendChild(pumpModeSelect);\n"
    "    grid.appendChild(pumpModeLabel);\n"
    "\n"
    "    const pumpValueLabel = document.createElement('label');\n"
    "    pumpValueLabel.textContent = 'Pump Value';\n"
    "    const pumpValueInput = document.createElement('input');\n"
    "    pumpValueInput.type = 'number';\n"
    "    pumpValueInput.step = 'any';\n"
    "    pumpValueInput.min = '0';\n"
    "    pumpValueInput.value = phase.pumpValue === '' ? '' : phase.pumpValue;\n"
    "    pumpValueInput.placeholder = pumpLabels[phase.pumpMode] || 'Value';\n"
    "    pumpValueInput.addEventListener('input', (event) => {\n"
    "      const value = event.target.value;\n"
    "      state.editingPhases[index].pumpValue = value === '' ? '' : Number(value);\n"
    "    });\n"
    "    pumpValueLabel.appendChild(pumpValueInput);\n"
    "    grid.appendChild(pumpValueLabel);\n"
    "\n"
    "    const temperatureLabel = document.createElement('label');\n"
    "    temperatureLabel.textContent = 'Temperature (°C)';\n"
    "    const temperatureInput = document.createElement('input');\n"
    "    temperatureInput.type = 'number';\n"
    "    temperatureInput.step = '0.1';\n"
    "    temperatureInput.value = phase.temperatureC === '' ? '' : phase.temperatureC;\n"
    "    temperatureInput.placeholder = '°C';\n"
    "    temperatureInput.addEventListener('input', (event) => {\n"
    "      const value = event.target.value;\n"
    "      state.editingPhases[index].temperatureC = value === '' ? '' : Number(value);\n"
    "    });\n"
    "    temperatureLabel.appendChild(temperatureInput);\n"
    "    grid.appendChild(temperatureLabel);\n"
    "\n"
    "    container.appendChild(grid);\n"
    "    phaseList.appendChild(container);\n"
    "  });\n"
    "  addPhaseButton.disabled = state.editingPhases.length >= MAX_PHASES;\n"
    "}\n"
    "\n"
    "async function setActive(index) {\n"
    "  try {\n"
    "    const response = await fetch('/api/profiles/active', {\n"
    "      method: 'PUT',\n"
    "      headers: { 'Content-Type': 'application/json' },\n"
    "      body: JSON.stringify({ index: index === null ? null : index })\n"
    "    });\n"
    "    if (!response.ok) {\n"
    "      const text = await response.text();\n"
    "      throw new Error(text || 'Failed to set active profile');\n"
    "    }\n"
    "    showMessage(index === null ? 'Active profile cleared' : 'Active profile updated');\n"
    "    await loadProfiles();\n"
    "  } catch (err) {\n"
    "    showMessage(err.message, true);\n"
    "  }\n"
    "}\n"
    "\n"
    "function startEditor(index) {\n"
    "  state.editingIndex = index;\n"
    "  editorCard.classList.remove('hidden');\n"
    "  const nameInput = editorForm.elements.namedItem('name');\n"
    "  if (index === -1) {\n"
    "    editorTitle.textContent = 'Add Profile';\n"
    "    nameInput.value = '';\n"
    "    state.editingPhases = [createNewPhase(0)];\n"
    "  } else {\n"
    "    const profile = state.profiles[index];\n"
    "    editorTitle.textContent = `Edit: ${profile.name}`;\n"
    "    nameInput.value = profile.name || '';\n"
    "    const phases = Array.isArray(profile.phases) ? profile.phases : [];\n"
    "    state.editingPhases = phases.length ? phases.map((phase, idx) => clonePhase(phase, idx)) : [createNewPhase(0)];\n"
    "  }\n"
    "  renderPhaseList();\n"
    "  nameInput.focus();\n"
    "}\n"
    "\n"
    "function hideEditor() {\n"
    "  state.editingIndex = null;\n"
    "  state.editingPhases = [];\n"
    "  editorCard.classList.add('hidden');\n"
    "  editorForm.reset();\n"
    "  phaseList.innerHTML = '';\n"
    "  phaseEmpty.classList.add('hidden');\n"
    "  addPhaseButton.disabled = false;\n"
    "}\n"
    "\n"
    "function renderProfiles() {\n"
    "  profileList.innerHTML = '';\n"
    "  const noneRow = document.createElement('div');\n"
    "  noneRow.className = 'profile-row';\n"
    "  const noneInfo = document.createElement('div');\n"
    "    noneInfo.className = 'profile-row-info';\n"
    "  const noneTitle = document.createElement('h3');\n"
    "  noneTitle.textContent = 'None';\n"
    "  noneInfo.appendChild(noneTitle);\n"
    "  const noneDesc = document.createElement('p');\n"
    "  noneDesc.textContent = 'Use manual settings.';\n"
    "  noneInfo.appendChild(noneDesc);\n"
    "  const noneActions = document.createElement('div');\n"
    "  noneActions.className = 'profile-row-actions';\n"
    "  const noneButton = createActionButton(state.activeIndex === null ? 'Active' : 'Activate', () => setActive(null), { disabled: state.activeIndex === null });\n"
    "  noneActions.appendChild(noneButton);\n"
    "  noneRow.appendChild(noneInfo);\n"
    "  noneRow.appendChild(noneActions);\n"
    "  profileList.appendChild(noneRow);\n"
    "  state.profiles.forEach((profile, index) => {\n"
    "    const row = document.createElement('div');\n"
    "    row.className = 'profile-row';\n"
    "    const info = document.createElement('div');\n"
    "    info.className = 'profile-row-info';\n"
    "    const title = document.createElement('h3');\n"
    "    title.textContent = profile.name;\n"
    "    info.appendChild(title);\n"
    "    const phaseCount = Array.isArray(profile.phases) ? profile.phases.length : (typeof profile.phaseCount === 'number' ? profile.phaseCount : 0);\n"
    "    const desc = document.createElement('p');\n"
    "    desc.textContent = `${phaseCount} phase${phaseCount === 1 ? '' : 's'}`;\n"
    "    info.appendChild(desc);\n"
    "    const actions = document.createElement('div');\n"
    "    actions.className = 'profile-row-actions';\n"
    "    const activateBtn = createActionButton(state.activeIndex === index ? 'Active' : 'Activate', () => setActive(index), { disabled: state.activeIndex === index });\n"
    "    actions.appendChild(activateBtn);\n"
    "    const editBtn = createActionButton('Edit', () => startEditor(index), { secondary: true });\n"
    "    actions.appendChild(editBtn);\n"
    "    row.appendChild(info);\n"
    "    row.appendChild(actions);\n"
    "    profileList.appendChild(row);\n"
    "  });\n"
    "  setActiveDisplay();\n"
    "}\n"
    "\n"
    "async function loadProfiles() {\n"
    "  try {\n"
    "    const response = await fetch('/api/profiles');\n"
    "    if (!response.ok) throw new Error('Failed to load profiles');\n"
    "    const data = await response.json();\n"
    "    state.profiles = Array.isArray(data.profiles) ? data.profiles : [];\n"
    "    if (Number.isInteger(data.activeIndex)) {\n"
    "      state.activeIndex = data.activeIndex;\n"
    "    } else {\n"
    "      state.activeIndex = null;\n"
    "    }\n"
    "    if (state.activeIndex !== null && state.activeIndex < 0) state.activeIndex = null;\n"
    "    renderProfiles();\n"
    "  } catch (err) {\n"
    "    showMessage(err.message, true);\n"
    "  }\n"
    "}\n"
    "\n"
    "editorForm.addEventListener('submit', async (event) => {\n"
    "  event.preventDefault();\n"
    "  const form = event.target;\n"
    "  const name = form.name.value.trim();\n"
    "  if (!name) {\n"
    "    showMessage('Name is required', true);\n"
    "    return;\n"
    "  }\n"
    "  if (state.editingPhases.length === 0) {\n"
    "    showMessage('At least one phase is required', true);\n"
    "    return;\n"
    "  }\n"
    "  const phases = [];\n"
    "  for (let i = 0; i < state.editingPhases.length; ++i) {\n"
    "    const phase = state.editingPhases[i];\n"
    "    const phaseName = typeof phase.name === 'string' ? phase.name.trim() : '';\n"
    "    if (!phaseName) {\n"
    "      showMessage(`Phase ${i + 1} name is required`, true);\n"
    "      return;\n"
    "    }\n"
    "    if (!durationModes.includes(phase.durationMode)) {\n"
    "      showMessage(`Phase ${i + 1} has an invalid duration mode`, true);\n"
    "      return;\n"
    "    }\n"
    "    if (phase.durationValue === '' || !Number.isFinite(Number(phase.durationValue)) || Number(phase.durationValue) <= 0) {\n"
    "      showMessage(`Phase ${i + 1} duration must be greater than 0`, true);\n"
    "      return;\n"
    "    }\n"
    "    if (!pumpModes.includes(phase.pumpMode)) {\n"
    "      showMessage(`Phase ${i + 1} has an invalid pump mode`, true);\n"
    "      return;\n"
    "    }\n"
    "    if (phase.pumpValue === '' || !Number.isFinite(Number(phase.pumpValue)) || Number(phase.pumpValue) < 0) {\n"
    "      showMessage(`Phase ${i + 1} pump value must be zero or greater`, true);\n"
    "      return;\n"
    "    }\n"
    "    if (phase.temperatureC === '' || !Number.isFinite(Number(phase.temperatureC))) {\n"
    "      showMessage(`Phase ${i + 1} temperature must be a number`, true);\n"
    "      return;\n"
    "    }\n"
    "    phases.push({\n"
    "      name: phaseName,\n"
    "      durationMode: phase.durationMode,\n"
    "      durationValue: Number(phase.durationValue),\n"
    "      pumpMode: phase.pumpMode,\n"
    "      pumpValue: Number(phase.pumpValue),\n"
    "      temperatureC: Number(phase.temperatureC)\n"
    "    });\n"
    "  }\n"
    "  const payload = { name, phases };\n"
    "  try {\n"
    "    let response;\n"
    "    if (state.editingIndex === -1) {\n"
    "      response = await fetch('/api/profiles', {\n"
    "        method: 'POST',\n"
    "        headers: { 'Content-Type': 'application/json' },\n"
    "        body: JSON.stringify(payload)\n"
    "      });\n"
    "    } else if (state.editingIndex !== null) {\n"
    "      response = await fetch(`/api/profiles/${state.editingIndex}`, {\n"
    "        method: 'PUT',\n"
    "        headers: { 'Content-Type': 'application/json' },\n"
    "        body: JSON.stringify(payload)\n"
    "      });\n"
    "    } else {\n"
    "      return;\n"
    "    }\n"
    "    if (!response.ok) {\n"
    "      const text = await response.text();\n"
    "      throw new Error(text || 'Failed to save profile');\n"
    "    }\n"
    "    showMessage('Profile saved');\n"
    "    hideEditor();\n"
    "    await loadProfiles();\n"
    "  } catch (err) {\n"
    "    showMessage(err.message, true);\n"
    "  }\n"
    "});\n"
    "\n"
    "cancelEditBtn.addEventListener('click', () => {\n"
    "  hideEditor();\n"
    "});\n"
    "\n"
    "addProfileButton.addEventListener('click', () => {\n"
    "  startEditor(-1);\n"
    "});\n"
    "\n"
    "addPhaseButton.addEventListener('click', () => {\n"
    "  if (state.editingPhases.length >= MAX_PHASES) {\n"
    "    return;\n"
    "  }\n"
    "  const nextIndex = state.editingPhases.length;\n"
    "  state.editingPhases.push(createNewPhase(nextIndex));\n"
    "  renderPhaseList();\n"
    "});\n"
    "\n"
    "hideEditor();\n"
    "loadProfiles();\n"
    "</script>\n"
    "\n"
    "</body>\n"
    "</html>\n";

static const char *duration_mode_to_string(BrewDurationMode mode)
{
    switch (mode)
    {
    case BREW_DURATION_TIME:
        return "time";
    case BREW_DURATION_VOLUME:
        return "volume";
    case BREW_DURATION_MASS:
        return "mass";
    default:
        return "unknown";
    }
}

static const char *pump_mode_to_string(BrewPumpMode mode)
{
    switch (mode)
    {
    case BREW_PUMP_POWER:
        return "power";
    case BREW_PUMP_PRESSURE:
        return "pressure";
    default:
        return "unknown";
    }
}

static bool parse_duration_mode(const cJSON *item, BrewDurationMode *out)
{
    if (cJSON_IsString(item) && item->valuestring)
    {
        if (strcasecmp(item->valuestring, "time") == 0)
        {
            *out = BREW_DURATION_TIME;
            return true;
        }
        if (strcasecmp(item->valuestring, "volume") == 0)
        {
            *out = BREW_DURATION_VOLUME;
            return true;
        }
        if (strcasecmp(item->valuestring, "mass") == 0)
        {
            *out = BREW_DURATION_MASS;
            return true;
        }
        return false;
    }
    if (cJSON_IsNumber(item))
    {
        int value = (int)item->valuedouble;
        if (value >= BREW_DURATION_TIME && value <= BREW_DURATION_MASS)
        {
            *out = (BrewDurationMode)value;
            return true;
        }
    }
    return false;
}

static bool parse_pump_mode(const cJSON *item, BrewPumpMode *out)
{
    if (cJSON_IsString(item) && item->valuestring)
    {
        if (strcasecmp(item->valuestring, "power") == 0)
        {
            *out = BREW_PUMP_POWER;
            return true;
        }
        if (strcasecmp(item->valuestring, "pressure") == 0)
        {
            *out = BREW_PUMP_PRESSURE;
            return true;
        }
        return false;
    }
    if (cJSON_IsNumber(item))
    {
        int value = (int)item->valuedouble;
        if (value >= BREW_PUMP_POWER && value <= BREW_PUMP_PRESSURE)
        {
            *out = (BrewPumpMode)value;
            return true;
        }
    }
    return false;
}

static esp_err_t parse_profile_json(const char *json, BrewProfileConfig *out, char *errbuf, size_t errlen)
{
    if (!json || !out)
        return ESP_ERR_INVALID_ARG;
    cJSON *root = cJSON_Parse(json);
    if (!root)
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Invalid JSON body", errlen);
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t result = ESP_OK;
    if (!cJSON_IsObject(root))
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Expected JSON object", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "name");
    cJSON *phases = cJSON_GetObjectItemCaseSensitive(root, "phases");
    if (!cJSON_IsString(name) || !name->valuestring)
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Profile name must be a string", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    if (!cJSON_IsArray(phases))
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "Phases must be an array", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    size_t phase_count = cJSON_GetArraySize(phases);
    if (phase_count == 0)
    {
        if (errbuf && errlen)
            strlcpy(errbuf, "At least one phase is required", errlen);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    if (phase_count > BREW_PROFILE_STORE_MAX_PHASES)
    {
        if (errbuf && errlen)
            snprintf(errbuf, errlen, "Maximum %u phases supported", BREW_PROFILE_STORE_MAX_PHASES);
        result = ESP_ERR_INVALID_ARG;
        goto cleanup;
    }
    memset(out, 0, sizeof(*out));
    strlcpy(out->name, name->valuestring, sizeof(out->name));
    out->phaseCount = (uint32_t)phase_count;
    for (uint32_t i = 0; i < out->phaseCount; ++i)
    {
        cJSON *phase_obj = cJSON_GetArrayItem(phases, i);
        if (!cJSON_IsObject(phase_obj))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u must be an object", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        BrewPhaseConfig *phase = &out->phases[i];
        cJSON *phase_name = cJSON_GetObjectItemCaseSensitive(phase_obj, "name");
        cJSON *duration_mode = cJSON_GetObjectItemCaseSensitive(phase_obj, "durationMode");
        cJSON *duration_value = cJSON_GetObjectItemCaseSensitive(phase_obj, "durationValue");
        cJSON *pump_mode = cJSON_GetObjectItemCaseSensitive(phase_obj, "pumpMode");
        cJSON *pump_value = cJSON_GetObjectItemCaseSensitive(phase_obj, "pumpValue");
        cJSON *temperature = cJSON_GetObjectItemCaseSensitive(phase_obj, "temperatureC");
        if (!cJSON_IsString(phase_name) || !phase_name->valuestring)
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u name must be a string", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!parse_duration_mode(duration_mode, &phase->durationMode))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u has invalid durationMode", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!cJSON_IsNumber(duration_value) || duration_value->valuedouble < 0.0)
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u durationValue must be non-negative number", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!parse_pump_mode(pump_mode, &phase->pumpMode))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u has invalid pumpMode", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!cJSON_IsNumber(pump_value))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u pumpValue must be a number", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        if (!cJSON_IsNumber(temperature))
        {
            if (errbuf && errlen)
                snprintf(errbuf, errlen, "Phase %u temperatureC must be a number", (unsigned)i);
            result = ESP_ERR_INVALID_ARG;
            goto cleanup;
        }
        strlcpy(phase->name, phase_name->valuestring, sizeof(phase->name));
        phase->durationValue = (uint32_t)(duration_value->valuedouble + 0.5);
        phase->pumpValue = (float)pump_value->valuedouble;
        phase->temperatureC = (float)temperature->valuedouble;
    }
cleanup:
    cJSON_Delete(root);
    return result;
}

static esp_err_t read_request_body(httpd_req_t *req, char **out_buf)
{
    if (!req || !out_buf)
        return ESP_ERR_INVALID_ARG;
    size_t total = req->content_len;
    if (total == 0)
        return ESP_ERR_INVALID_SIZE;
    char *buf = calloc(1, total + 1);
    if (!buf)
        return ESP_ERR_NO_MEM;
    size_t received = 0;
    while (received < total)
    {
        int ret = httpd_req_recv(req, buf + received, total - received);
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
                continue;
            free(buf);
            return ESP_FAIL;
        }
        received += ret;
    }
    buf[received] = '\0';
    *out_buf = buf;
    return ESP_OK;
}

static esp_err_t send_json_response(httpd_req_t *req, cJSON *json)
{
    char *payload = cJSON_PrintUnformatted(json);
    if (!payload)
        return ESP_ERR_NO_MEM;
    httpd_resp_set_type(req, "application/json");
    esp_err_t err = httpd_resp_send(req, payload, HTTPD_RESP_USE_STRLEN);
    free(payload);
    return err;
}

static esp_err_t handle_get_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_get_profiles(httpd_req_t *req)
{
    BrewProfileSnapshot *snapshot = calloc(1, sizeof(*snapshot));
    if (!snapshot)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    esp_err_t err = BrewProfileStore_GetSnapshot(snapshot);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get profiles: %s", esp_err_to_name(err));
        free(snapshot);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load profiles");
    }
    int32_t active_index = BREW_PROFILE_STORE_ACTIVE_NONE;
    err = BrewProfileStore_GetActiveProfile(&active_index);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get active profile: %s", esp_err_to_name(err));
        free(snapshot);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to load profiles");
    }
    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        free(snapshot);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    cJSON *profiles = cJSON_CreateArray();
    if (!profiles)
    {
        cJSON_Delete(root);
        free(snapshot);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    cJSON_AddItemToObject(root, "profiles", profiles);
    for (uint32_t i = 0; i < snapshot->profileCount; ++i)
    {
        const BrewProfileConfig *profile = &snapshot->profiles[i];
        cJSON *profile_obj = cJSON_CreateObject();
        if (!profile_obj)
            goto error;
        cJSON_AddStringToObject(profile_obj, "name", profile->name);
        cJSON_AddNumberToObject(profile_obj, "phaseCount", profile->phaseCount);
        cJSON *phases = cJSON_CreateArray();
        if (!phases)
        {
            cJSON_Delete(profile_obj);
            goto error;
        }
        cJSON_AddItemToObject(profile_obj, "phases", phases);
        for (uint32_t p = 0; p < profile->phaseCount; ++p)
        {
            const BrewPhaseConfig *phase = &profile->phases[p];
            cJSON *phase_obj = cJSON_CreateObject();
            if (!phase_obj)
                goto error;
            cJSON_AddStringToObject(phase_obj, "name", phase->name);
            cJSON_AddStringToObject(phase_obj, "durationMode", duration_mode_to_string(phase->durationMode));
            cJSON_AddNumberToObject(phase_obj, "durationValue", phase->durationValue);
            cJSON_AddStringToObject(phase_obj, "pumpMode", pump_mode_to_string(phase->pumpMode));
            cJSON_AddNumberToObject(phase_obj, "pumpValue", phase->pumpValue);
            cJSON_AddNumberToObject(phase_obj, "temperatureC", phase->temperatureC);
            cJSON_AddItemToArray(phases, phase_obj);
        }
        cJSON_AddItemToArray(profiles, profile_obj);
    }
    if (active_index >= 0)
    {
        cJSON_AddNumberToObject(root, "activeIndex", active_index);
    }
    else
    {
        cJSON_AddNullToObject(root, "activeIndex");
    }
    err = send_json_response(req, root);
    cJSON_Delete(root);
    free(snapshot);
    return err;
error:
    cJSON_Delete(root);
    free(snapshot);
    return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to encode profiles");
}

static esp_err_t handle_post_profiles(httpd_req_t *req)
{
    char *body = NULL;
    esp_err_t err = read_request_body(req, &body);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NO_MEM)
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
    }
    BrewProfileConfig *profile = calloc(1, sizeof(*profile));
    if (!profile)
    {
        free(body);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    char error_msg[96];
    err = parse_profile_json(body, profile, error_msg, sizeof(error_msg));
    free(body);
    if (err != ESP_OK)
    {
        free(profile);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error_msg);
    }
    uint32_t index = 0;
    err = BrewProfileStore_AddProfile(profile, &index);
    free(profile);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add profile: %s", esp_err_to_name(err));
        if (is_storage_full_error(err))
        {
            httpd_resp_set_status(req, "507 Insufficient Storage");
            httpd_resp_set_type(req, "text/plain");
            return httpd_resp_send(req, "Profile storage full", HTTPD_RESP_USE_STRLEN);
        }
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save profile");
    }
    cJSON *response = cJSON_CreateObject();
    if (!response)
    {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    cJSON_AddNumberToObject(response, "index", index);
    err = send_json_response(req, response);
    cJSON_Delete(response);
    return err;
}

static esp_err_t handle_put_profiles(httpd_req_t *req)
{
    const char *uri = req->uri;
    const char *prefix = "/api/profiles/";
    size_t prefix_len = strlen(prefix);
    if (strncmp(uri, prefix, prefix_len) != 0)
    {
        return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Not found");
    }
    uint32_t index = (uint32_t)strtoul(uri + prefix_len, NULL, 10);
    char *body = NULL;
    esp_err_t err = read_request_body(req, &body);
    if (err != ESP_OK)
    {
        if (err == ESP_ERR_NO_MEM)
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
    }
    BrewProfileConfig *profile = calloc(1, sizeof(*profile));
    if (!profile)
    {
        free(body);
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    }
    char error_msg[96];
    err = parse_profile_json(body, profile, error_msg, sizeof(error_msg));
    free(body);
    if (err != ESP_OK)
    {
        free(profile);
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, error_msg);
    }
    err = BrewProfileStore_UpdateProfile(index, profile);
    free(profile);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to update profile %u: %s", (unsigned)index, esp_err_to_name(err));
        if (is_storage_full_error(err))
        {
            httpd_resp_set_status(req, "507 Insufficient Storage");
            httpd_resp_set_type(req, "text/plain");
            return httpd_resp_send(req, "Profile storage full", HTTPD_RESP_USE_STRLEN);
        }
        if (err == ESP_ERR_INVALID_ARG)
            return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Profile not found");
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to update profile");
    }
    cJSON *response = cJSON_CreateObject();
    if (!response)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    cJSON_AddStringToObject(response, "status", "ok");
    err = send_json_response(req, response);
    cJSON_Delete(response);
    return err;
}

static esp_err_t handle_put_active_profile(httpd_req_t *req)
{
    int32_t desired_index = BREW_PROFILE_STORE_ACTIVE_NONE;
    char *body = NULL;
    if (req->content_len > 0)
    {
        esp_err_t err = read_request_body(req, &body);
        if (err != ESP_OK)
        {
            if (err == ESP_ERR_NO_MEM)
                return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request body");
        }
        cJSON *json = cJSON_Parse(body);
        free(body);
        body = NULL;
        if (!json)
        {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON body");
        }
        if (!cJSON_IsObject(json))
        {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected JSON object");
        }
        cJSON *index_item = cJSON_GetObjectItemCaseSensitive(json, "index");
        if (!index_item || cJSON_IsNull(index_item))
        {
            desired_index = BREW_PROFILE_STORE_ACTIVE_NONE;
        }
        else if (cJSON_IsNumber(index_item))
        {
            desired_index = (int32_t)index_item->valuedouble;
        }
        else
        {
            cJSON_Delete(json);
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "index must be a number or null");
        }
        cJSON_Delete(json);
    }

    esp_err_t err = BrewProfileStore_SetActiveProfile(desired_index);
    if (err == ESP_ERR_INVALID_ARG)
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid profile index");
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set active profile: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to set active profile");
    }

    int32_t stored_index = BREW_PROFILE_STORE_ACTIVE_NONE;
    err = BrewProfileStore_GetActiveProfile(&stored_index);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to confirm active profile: %s", esp_err_to_name(err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read active profile");
    }

    cJSON *response = cJSON_CreateObject();
    if (!response)
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
    if (stored_index >= 0)
    {
        cJSON_AddNumberToObject(response, "activeIndex", stored_index);
    }
    else
    {
        cJSON_AddNullToObject(response, "activeIndex");
    }
    cJSON_AddStringToObject(response, "status", "ok");
    err = send_json_response(req, response);
    cJSON_Delete(response);
    return err;
}

esp_err_t WebServer_Init(void)
{
    if (s_initialized)
        return ESP_OK;
    esp_err_t err = BrewProfileStore_Init();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialise profile store: %s", esp_err_to_name(err));
        return err;
    }
    s_initialized = true;
    return ESP_OK;
}

esp_err_t WebServer_Start(void)
{
    if (!s_initialized)
        return ESP_ERR_INVALID_STATE;
    if (s_server)
        return ESP_OK;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.uri_match_fn = httpd_uri_match_wildcard;
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }
    httpd_uri_t root_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_get_root,
        .user_ctx = NULL,
    };
    httpd_uri_t index_uri = {
        .uri = "/index.html",
        .method = HTTP_GET,
        .handler = handle_get_root,
        .user_ctx = NULL,
    };
    httpd_uri_t profiles_get = {
        .uri = "/api/profiles",
        .method = HTTP_GET,
        .handler = handle_get_profiles,
        .user_ctx = NULL,
    };
    httpd_uri_t profiles_post = {
        .uri = "/api/profiles",
        .method = HTTP_POST,
        .handler = handle_post_profiles,
        .user_ctx = NULL,
    };
    httpd_uri_t profiles_active_put = {
        .uri = "/api/profiles/active",
        .method = HTTP_PUT,
        .handler = handle_put_active_profile,
        .user_ctx = NULL,
    };
    httpd_uri_t profiles_put = {
        .uri = "/api/profiles/*",
        .method = HTTP_PUT,
        .handler = handle_put_profiles,
        .user_ctx = NULL,
    };
    httpd_register_uri_handler(s_server, &root_uri);
    httpd_register_uri_handler(s_server, &index_uri);
    httpd_register_uri_handler(s_server, &profiles_get);
    httpd_register_uri_handler(s_server, &profiles_post);
    httpd_register_uri_handler(s_server, &profiles_active_put);
    httpd_register_uri_handler(s_server, &profiles_put);
    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

void WebServer_Stop(void)
{
    if (s_server)
    {
        httpd_stop(s_server);
        s_server = NULL;
        ESP_LOGI(TAG, "HTTP server stopped");
    }
}
