/*******************************************************************************/

function handleHttpErrors(response) {
    if (!response.ok) {
        throw Error(response.status);
    }
    return response;
}

/*******************************************************************************/

async function getUrl(url, reload = false) {
    let options = {}
    if (reload) {
        options = { cache: "reload" }
    }

    try {
        return await fetch(url, options).then(handleHttpErrors);
    }
    catch (err) {
        throw new Error(`Erreur lors de la r\u00E9cup\u00E9ration de l'URL "${url}" : ${err}`);
    }
}

async function postUrlJson(url, json, headers = {}) {
    return await postUrl(url, JSON.stringify(json), headers);
}

async function postUrl(url, body, headers = {}) {
    try {
        return await fetch(url, {
            method: 'POST',
            headers: headers,
            body: body
        }).then(handleHttpErrors);
    }
    catch (err) {
        throw new Error(`Erreur lors de l'envoi de l'URL "${url}" : ${err}`);
    }
}

async function deleteUrl(url, headers = {}) {
    try {
        return await fetch(url, {
            method: 'DELETE',
            headers: headers,
        }).then(handleHttpErrors);
    }
    catch (err) {
        throw new Error(`Erreur lors de la suppression de l'URL "${url}" : ${err}`);
    }
}

async function putUrlJson(url, json, headers = {}) {
    return await putUrl(url, JSON.stringify(json), headers);
}

async function putUrl(url, body, headers = {}) {
    try {
        return await fetch(url, {
            method: 'PUT',
            headers: headers,
            body: body
        }).then(handleHttpErrors);
    }
    catch (err) {
        throw new Error(`Erreur lors de la mise \u00E0 jour de l'URL "${url}" : ${err}`);
    }
}

/*******************************************************************************/

function promptNonEmptyString(message) {
    let result = window.prompt(message);
    if (result === null) return null;
    result = result.trim();
    if (result.length === 0) return null;
    return result;
}

/*******************************************************************************/

function logError(message) {
    let msg = document.createElement("div");
    let txt = document.createTextNode(message);
    msg.appendChild(txt);

    let el = document.getElementById('errors');
    el.appendChild(msg);
    el.className = 'mb-3 p-3 bg-warning';
}

/*******************************************************************************/

function secondsToDuration(s) {
    const d = Math.floor(s / (3600 * 24));
    s -= d * 3600 * 24;
    const h = Math.floor(s / 3600);
    s -= h * 3600;
    const m = Math.floor(s / 60);
    s -= m * 60;
    const tmp = [];
    (d) && tmp.push(d + 'j');
    (d || h) && tmp.push(h + 'h');
    (d || h || m) && tmp.push(m + 'm');
    tmp.push(s + 's');
    return tmp.join(' ');
}

/*******************************************************************************/

async function apiGetStatusJson(reload = false) {
    let statusResponse = await getUrl('samples/status.json', reload);
    let statusJson = await statusResponse.json();
    return statusJson;
}

async function loadStatus() {
    let status = await apiGetStatusJson();

    let el = document.getElementById('status');

    let d = document.createElement('div');
    d.innerHTML = `Syst\u00E8me d\u00E9mar\u00E9 depuis ${secondsToDuration(status.sytem_uptime)}`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `Connect\u00E9 au Wifi depuis ${secondsToDuration(status.connection_uptime)}`;
    el.appendChild(d);
}

async function changeZoneOverrides(override) {
    console.log("changeZoneOverrides", override);
    putUrlJson('/api/v1/override', { order: override }).catch(logError);
    loadZoneOverrides().catch(logError);
}

async function apiGetOrderTypesJson(reload = false) {
    let orderTypesResponse = await getUrl('samples/orders.json', reload);
    let orderTypesJson = await orderTypesResponse.json();
    return orderTypesJson.orders;
}

async function apiGetZoneOverrideJson(reload = false) {
    let zoneOverrideResponse = await getUrl('samples/zones_override.json', reload);
    let zoneOverrideJson = await zoneOverrideResponse.json();
    return zoneOverrideJson.override;
}

async function loadZoneOverrides(reload = false) {
    let zoneOverrideId = await apiGetZoneOverrideJson(reload);
    let orderTypesSupported = await apiGetOrderTypesJson(reload);

    let noOverride = { id: 'none', name: 'Aucun for&ccedil;age', class: 'primary' }
    let overrideTypesJsonAll = [noOverride, ...orderTypesSupported];

    let templateLabel = {
        '<>': 'label',
        'class': 'btn btn-outline-${class} w-100',
        'for': 'zoneOverride_${id}',
        'onclick': function (e) {
            changeZoneOverrides(e.obj.id);
        },
        'html': '${name}'
    };

    let templateInput = {
        '<>': 'input',
        'type': 'radio',
        'class': 'btn-check w-100',
        'name': 'overrideZones',
        'id': 'zoneOverride_${id}',
        'autocomplete': 'off'
    };

    let templateMaster = {
        '<>': 'div', 'class': 'col-sm-auto mb-3', 'html': [
            templateInput,
            templateLabel
        ]
    };

    // clear content
    document.getElementById('zoneOverride').textContent = '';

    // NOTE: json2html requires jquery to insert event handlers
    $('#zoneOverride').json2html(overrideTypesJsonAll, templateMaster);

    let el = document.getElementById(`zoneOverride_${zoneOverrideId}`)
    el.toggleAttribute("checked", true);
}

/*******************************************************************************/

async function changeZoneDescription(zoneId) {
    console.log("changeZoneDescription", zoneId);
    let name = promptNonEmptyString(`Entrez le nouveu nom de la zone '${zoneId}'`);
    postUrlJson(`/api/v1/zone/${zoneId}/description`, { description: name }).catch(logError);
    loadZoneConfiguration().catch(logError);
}

async function changeZoneValue(zoneId, value) {
    console.log("changeZoneValue", zoneId, value);
    postUrlJson(`/api/v1/zone/${zoneId}/mode`, { mode: value }).catch(logError);
    loadZoneConfiguration().catch(logError);
}

async function apiGetZoneConfigJson(reload = false) {
    let zoneConfigResponse = await getUrl('samples/zones_config.json', reload);
    let zoneConfigJson = await zoneConfigResponse.json();
    return zoneConfigJson.zones;
}

async function apiGetZoneStateJson(reload = false) {
    let zoneStateResponse = await getUrl('samples/zones_state.json', reload);
    let zoneStateJson = await zoneStateResponse.json();
    return zoneStateJson;
}

async function loadZoneConfiguration(reload = false) {
    let orderTypesSupported = await apiGetOrderTypesJson(reload);
    let zoneConfig = await apiGetZoneConfigJson(reload);
    let zoneState = await apiGetZoneStateJson(reload);
    let orderTypes = await apiGetOrderTypesJson(reload);
    let planningList = await apiGetPlanningListJson(reload);

    let optionsHtml = json2html.render(orderTypes, { '<>': 'option', 'value': ':fixed:${id}', 'html': 'Fixe: ${name}' })
        + json2html.render(planningList, { '<>': 'option', 'value': ':planning:${id}', 'html': 'Programmation: ${name}' });

    let orderById = function (id) {
        return orderTypesSupported.find(el => el.id === zoneState[id]);
    }

    let template = {
        '<>': 'div', 'class': 'row mb-3 d-flex align-items-center', 'html': [
            {
                '<>': 'div', 'class': 'col mb-3', 'html': '<b>${desc}</b> (${id})'
            },
            {
                '<>': 'div', 'class': function () {
                    let o = orderById(this.id);
                    return `col mb-3 bg-${o.class}`;
                }, 'html': function () {
                    let o = orderById(this.id);
                    return `Etat actuel: ${o.name}`;
                }
            },
            {
                '<>': 'div', 'class': 'col-auto mb-3', 'html': [
                    {
                        '<>': 'button',
                        'class': 'btn btn-warning btn',
                        'onclick': function (e) {
                            changeZoneDescription(e.obj.id);
                        },
                        'html': 'Renommer'
                    },
                ]
            },
            {
                '<>': 'div', 'class': 'col-sm mb-3', 'html': [
                    {
                        '<>': 'select',
                        'class': 'form-select',
                        'id': 'select_zone_${id}',
                        'onchange': function (e) {
                            changeZoneValue(e.obj.id, e.event.currentTarget.value);
                        },
                        'html': optionsHtml
                    }
                ]
            },
        ]
    }

    // NOTE: json2html requires jquery to insert event handlers
    $('#zoneList').json2html(zoneConfig, template);

    zoneConfig.forEach((zone) => {
        let el = document.getElementById(`select_zone_${zone.id}`);
        el.value = zone.mode;
    });
}

/*******************************************************************************/

async function apiGetPlanningListJson(reload = false) {
    let planningListResponse = await getUrl('samples/plannings.json', reload);
    let planningListJson = await planningListResponse.json();
    return planningListJson.plannings;
}

async function initPlanningCreate() {
    let b = document.getElementById('planningCreateButton');
    b.onclick = createPlanning;
}

async function createPlanning() {
    console.log('createPlanning');
    let name = promptNonEmptyString('Entrez le nom du nouveau planning');
    postUrlJson(`/api/v1/planning`, { name: name }).catch(logError);
    loadPlanningList().catch(logError);
}

async function renamePlanning(id) {
    console.log('renamePlanning', id);
    let name = promptNonEmptyString('Entrez le nouveau nom du planning');
    postUrlJson(`/api/v1/planning/${id}/name`, { name: name }).catch(logError);
    loadPlanningList().catch(logError);
}

async function deletePlanning(id) {
    console.log('deletePlanning', id);
    deleteUrl(`/api/v1/planning/${id}`).catch(logError);
    loadPlanningList().catch(logError);
}

function getSelectedPlanning() {
    let el = document.getElementById('planningSelect');
    return el.value;
}

async function loadPlanningList(reload = false) {
    let planningList = await apiGetPlanningListJson(reload);

    let template = { '<>': 'option', 'value': '${id}', 'html': '${name}' };

    let el = document.getElementById('planningSelect');
    el.innerHTML = json2html.render(planningList, template);
    el.onchange = function (e) {
        // action after initial loading : ignore cache for fresh data
        loadPlanningDetails(this.value, true);
    };

    el = document.getElementById('planningRenameButton');
    el.onclick = function (e) {
        let el = document.getElementById('planningSelect');
        renamePlanning(el.value);
    };

    el = document.getElementById('planningDeleteButton');
    el.onclick = function (e) {
        let el = document.getElementById('planningSelect');
        deletePlanning(el.value);
    };

    let planningId = getSelectedPlanning();
    if (planningId) {
        // action during initial loading : use cache if possible
        loadPlanningDetails(planningId);
    }
}

/*******************************************************************************/

async function apiGetPlanningDetailsJson(reload = false) {
    let planningDetailsResponse = await getUrl('samples/planning_details.json', reload);
    let planningDetailsJson = await planningDetailsResponse.json();
    return planningDetailsJson.slots;
}

async function changePlanningDetailMode(planningId, start, newMode) {
    console.log('changePlanningDetailMode', planningId, start, newMode);
    let startId = start.replace(':', '');
    postUrlJson(`/api/v1/planning/${planningId}/details/${startId}/mode`, { mode: newMode }).catch(logError);
    loadPlanningDetails(planningId).catch(logError);
}

async function changePlanningDetailStart(planningId, start, newStart) {
    console.log('changePlanningDetailStart', planningId, start, newStart);
    let startId = start.replace(':', '');
    postUrlJson(`/api/v1/planning/${planningId}/details/${startId}/start`, { new_start: newStart }).catch(logError);
    loadPlanningDetails(planningId).catch(logError);
}

async function deletePlanningDetail(planningId, start) {
    console.log('deletePlanningDetail', planningId, start);
    let startId = start.replace(':', '');
    deleteUrl(`/api/v1/planning/${planningId}/details/${startId}`).catch(logError);
    loadPlanningDetails().catch(logError);
}

async function addPlanningDetailSlot(planningId) {
    console.log('addPlanningDetailSlot', planningId);
    postUrlJson(`/api/v1/planning/${planningId}/append`).catch(logError);
    loadPlanningDetails().catch(logError);
}

async function loadPlanningDetails(planningId, reload = false) {
    let slots = await apiGetPlanningDetailsJson();

    let orderTypes = await apiGetOrderTypesJson(reload);

    let optionsHtml = json2html.render(orderTypes, { '<>': 'option', 'value': ':fixed:${id}', 'html': '${name}' });

    let template = {
        '<>': 'div', 'class': 'row mb-3', 'html': [
            {
                '<>': 'div', 'class': 'col', 'html': [
                    {
                        '<>': 'input',
                        'class': 'form-control',
                        'type': 'text',
                        'value': '${start}',
                        'onchange': function (e) {
                            changePlanningDetailStart(planningId, e.obj.start, e.event.currentTarget.value);
                        }
                    },
                ]
            },
            {
                '<>': 'div', 'class': 'col', 'html': [
                    {
                        '<>': 'select',
                        'id': 'planningDetailSelect_${start}',
                        'class': 'form-select',
                        'onchange': function (e) {
                            changePlanningDetailMode(planningId, e.obj.start, e.event.currentTarget.value);
                        },
                        'html': optionsHtml
                    }
                ]
            },
            {
                '<>': 'div', 'class': 'col-auto', 'html': [
                    {
                        '<>': 'button', 'class': 'btn btn-danger',
                        'onclick': function (e) {
                            deletePlanningDetail(planningId, e.obj.start);
                        },
                        'html': [{ '<>': 'span', 'class': 'bi bi-trash' }]
                    }
                ]
            }
        ]
    };

    // NOTE: json2html requires jquery to insert event handlers
    $('#planningDetails').json2html(slots, template);

    slots.forEach(function (slot) {
        let e = document.getElementById(`planningDetailSelect_${slot.start}`);
        e.value = `:fixed:${slot.order}`;
    });

    let el = document.getElementById('planningDetailsAdd');
    el.onclick = function (e) {
        addPlanningDetailSlot(planningId);
    };
}

/**************************************************************************/

async function accountCreate(userId, cleartextPassword) {
    console.log('accountCreate', userId, cleartextPassword);
    userId = userId.trim();
    if (userId.length === 0) return null;
    cleartextPassword = userId.trim();
    if (cleartextPassword.length === 0) return null;
    postUrlJson('/api/v1/user', { id: userId, password: cleartextPassword }).catch(logError);
    loadAccounts().catch(logError);
}

async function initAccountCreate() {
    let el = document.getElementById('newAccountButton');
    el.onclick = function (e) {
        let e1 = document.getElementById('newAccountId');
        let e2 = document.getElementById('newAccountPass');
        accountCreate(e1.value, e2.value);
    };
}

async function accountDelete(userId) {
    console.log('accountDelete', userId);
    userId = userId.trim();
    if (userId.length === 0) return null;
    deleteUrl(`/api/v1/user/${userId}`).catch(logError);
    loadAccounts().catch(logError);
}

async function accountPasswordReset(userId) {
    console.log('accountPasswordReset', userId);
    let password = promptNonEmptyString(`Entrez le nouveu password du compte '${userId}'`);
    postUrlJson(`/api/v1/user/${userId}/password`, { password: password }).catch(logError);
}

async function loadAccounts() {
    let accounts = await apiGetAccountsJson();

    let template = {
        '<>': 'tr', 'html': [
            { '<>': 'th', 'html': '${id}' },
            { '<>': 'td', 'html': '${type}' },
            {
                '<>': 'td', 'html': [
                    {
                        '<>': 'button', 'class': 'btn btn-warning',
                        'onclick': function (e) {
                            accountPasswordReset(e.obj.id);
                        },
                        'html': [{ '<>': 'span', 'class': 'bi bi-pencil-square' }]
                    }
                ]
            },
            {
                '<>': 'td', 'html': function () {
                    if (this.type === 'admin') return;
                    // NOTE: json2html requires jquery to insert event handlers
                    return $.json2html(this, {
                        '<>': 'button', 'class': 'btn btn-danger',
                        'onclick': function (e) {
                            accountDelete(e.obj.id);
                        },
                        'html': [{ '<>': 'span', 'class': 'bi bi-trash' }]
                    });
                }
            }
        ]
    };

    let el = document.getElementById('accountListTable');
    el.innerHTML = '<tr><th>Nom</th><th>Type</th><th>MdP</th><th>Suppr.</th></tr>';

    // NOTE: json2html requires jquery to insert event handlers
    $('#accountListTable').json2html(accounts, template);
}

async function apiGetAccountsJson(reload = false) {
    let accountsResponse = await getUrl('samples/accounts.json', reload);
    let accountsJson = await accountsResponse.json();
    return accountsJson.accounts;
}

/*******************************************************************************/

async function uploadFirmware(file) {
    console.log("uploadFirmware", file);
    let formData = new FormData();
    formData.set('file', file);
    postUrl('/api/v1/upgrade', formData).catch(logError);
}

async function initFirmwareButtons() {
    el = document.getElementById('updateUploadButton');
    el.onclick = function (e) {
        let t = document.getElementById('updateTextFilePath');
        if (t.files.length != 1) return;
        uploadFirmware(t.files[0]);
    }
}

/*******************************************************************************/

async function apiGetHardwareSupportedJson(reload = false) {
    let hardwareSupportedResponse = await getUrl('samples/hardware_supported.json', reload);
    let hardwareSupportedJson = await hardwareSupportedResponse.json();
    return hardwareSupportedJson.supported;
}

async function apiGetHardwareCurrentJson(reload = false) {
    let hardwareCurrentResponse = await getUrl('samples/hardware_current.json', reload);
    let hardwareCurrentJson = await hardwareCurrentResponse.json();
    return hardwareCurrentJson;
}

async function apiGetHardwareParamsJson(hardwareId, reload = false) {
    let hardwareParamsResponse = await getUrl('samples/hardware_params.json', reload);
    let hardwareParamsJson = await hardwareParamsResponse.json();
    return hardwareParamsJson;
}

async function loadHardwareSupported() {
    let hardwareSupported = await apiGetHardwareSupportedJson();
    let hardwareCurrent = await apiGetHardwareCurrentJson();

    let template = { '<>': 'option', 'value': '${id}', 'html': '${name}' };

    let el = document.getElementById('hardwareSupportedSelect');
    el.innerHTML = json2html.render(hardwareSupported, template);
    el.onchange = function (e) {
        // action after initial loading : ignore cache for fresh data
        loadHardwareParameters(this.value);
    };

    el.value = hardwareCurrent.id;
    loadHardwareParameters(hardwareCurrent.id);
}

async function initHardwareParametersButtons() {
    let el = document.getElementById('hardwareCurrentReloadButton');
    el.onclick = function (e) {
        loadHardwareSupported();
    }

    el = document.getElementById('hardwareCurrentApplyButton');
    el.onclick = function (e) {
        let status = confirm('La centrale de chauffage doit red\u00E9marrer pour prendre en compte les nouveux param\u00EAtres mat\u00E9riels.');
        if (status === true) {
            let f = document.getElementById('hardwareCurrentForm');
            f.submit();
        }
    }
}

async function loadHardwareParameters(hardwareId) {
    console.log('loadHardwareParameters', hardwareId);

    let hardwareParams = await apiGetHardwareParamsJson(hardwareId);

    let template = {
        '<>': 'div', 'class': 'form-floating mb-3', 'html': [
            { '<>': 'input', 'type': '${type}', 'class': 'form-control', 'id': 'hardware_parameter_id_${id}', 'value': '${value}' },
            { '<>': 'label', 'for': 'hardware_parameter_id_${id}', 'html': '${description}' }
        ]
    };

    let el = document.getElementById('hardwareCurrentParameters');
    el.innerHTML = json2html.render(hardwareParams.parameters, template);
}

/*******************************************************************************/

async function ofp_init() {
    Promise.all([
        loadStatus(),
        loadZoneOverrides(),
        loadZoneConfiguration(),
        initPlanningCreate(),
        loadPlanningList(),
        initAccountCreate(),
        loadAccounts(),
        initFirmwareButtons(),
        loadHardwareSupported(),
        initHardwareParametersButtons(),
    ]).catch(logError);
}

window.onload = ofp_init
