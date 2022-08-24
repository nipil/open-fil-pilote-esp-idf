/*******************************************************************************/

function handleHttpErrors(response) {
    if (!response.ok) {
        throw Error(response.status);
    }
    return response;
}

/*******************************************************************************/

async function doUrl(url, errorMessage, options = {}) {
    // let options = { method: 'POST' }
    // if (body) options.body = body;
    // if (headers) options.headers = headers;
    // if (reload) options.cache = 'reload';
    try {
        return await fetch(url, options).then(handleHttpErrors);
    }
    catch (err) {
        throw new Error(`${errorMessage} ${url} : ${err}`);
    }
}

async function getUrlJson(url, options = {}) {
    let result = await getUrl(url, options);
    let json = await result.json();
    return json;
}

async function getUrl(url, options = {}) {
    options.method = 'GET';
    return await doUrl(url, 'Erreur lors de la r\u00E9cup\u00E9ration', options);
}

async function postUrlJson(url, json, options = {}) {
    return await postUrl(url, JSON.stringify(json), options);
}

async function postUrl(url, body, options = {}) {
    options.method = 'POST';
    options.body = body;
    return await doUrl(url, 'Erreur lors de la cr\u00E9ation', options);
}

async function deleteUrl(url, options = {}) {
    options.method = 'DELETE';
    return await doUrl(url, 'Erreur lors de la suppression', options);
}

async function putUrlJson(url, json, options = {}) {
    return await putUrl(url, JSON.stringify(json), options);
}

async function putUrl(url, body, options = {}) {
    options.method = 'PUT';
    options.body = body;
    return await doUrl(url, 'Erreur lors de la mise \u00E0 jour', options);
}

async function patchUrlJson(url, json, options = {}) {
    return await patchUrl(url, JSON.stringify(json), options);
}

async function patchUrl(url, body, options = {}) {
    options.method = 'PATCH';
    options.body = body;
    return await doUrl(url, 'Erreur lors de la mise \u00E0 jour partielle', options);
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

function logError(err) {
    console.warn(err);
    let msg = document.createElement("div");
    let txt = document.createTextNode(err);
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

async function apiGetStatusJson() {
    return await getUrlJson('/ofp-api/v1/status');
}

async function loadStatus() {
    let { uptime } = await apiGetStatusJson();

    let el = document.getElementById('status');

    let d = document.createElement('div');
    d.innerHTML = `Syst\u00E8me d\u00E9mar\u00E9 depuis ${secondsToDuration(uptime.system)}`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `Connect\u00E9 au Wifi depuis ${secondsToDuration(uptime.wifi)}`;
    el.appendChild(d);
}

/*******************************************************************************/

async function changeZoneOverrides(override) {
    console.log("changeZoneOverrides", override);
    putUrlJson('/ofp-api/v1/override', { override: override }).catch(logError);
    loadZoneOverrides().catch(logError);
}

async function apiGetOrderTypesJson() {
    return await getUrlJson('/ofp-api/v1/orders');
}

async function apiGetZoneOverrideJson() {
    return await getUrlJson('/ofp-api/v1/override');
}

async function loadZoneOverrides() {
    let { override } = await apiGetZoneOverrideJson();
    let { orders } = await apiGetOrderTypesJson();
    let noOverride = { id: 'none', name: 'Aucun for&ccedil;age', class: 'primary' }
    let overrideTypesJsonAll = [noOverride, ...orders];

    let templateLabel = {
        '<>': 'label',
        'class': 'btn btn-outline-${class} w-100',
        'for': 'zoneOverride_${id}',
        'onclick': async function (e) {
            await changeZoneOverrides(e.obj.id);
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

    let el = document.getElementById(`zoneOverride_${override}`)
    el.toggleAttribute("checked", true);
}

/*******************************************************************************/

async function changeZoneDescription(zoneId) {
    console.log("changeZoneDescription", zoneId);
    let name = promptNonEmptyString(`Entrez le nouveu nom de la zone '${zoneId}'`);
    patchUrlJson(`/ofp-api/v1/zones/${zoneId}`, { desc: name }).catch(logError);
    loadZoneConfiguration().catch(logError);
}

async function changeZoneValue(zoneId, value) {
    console.log("changeZoneValue", zoneId, value);
    patchUrlJson(`/ofp-api/v1/zones/${zoneId}`, { mode: value }).catch(logError);
    loadZoneConfiguration().catch(logError);
}

async function apiGetZonesJson() {
    return await getUrlJson('/ofp-api/v1/zones');
}

async function loadZoneConfiguration() {
    let { orders } = await apiGetOrderTypesJson();
    let { zones } = await apiGetZonesJson();
    let { plannings } = await apiGetPlanningListJson();

    let optionsHtml = json2html.render(orders, { '<>': 'option', 'value': ':fixed:${id}', 'html': 'Fixe: ${name}' })
        + json2html.render(plannings, { '<>': 'option', 'value': ':planning:${id}', 'html': 'Programmation: ${name}' });

    let orderById = function (id) {
        // find zone info according to iteration zoneId
        let zone = zones.find(z => z.id === id);
        // find order according to current orderId of zone
        return orders.find(el => el.id === zone.current);
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
                        'onclick': async function (e) {
                            await changeZoneDescription(e.obj.id);
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
                        'onchange': async function (e) {
                            await changeZoneValue(e.obj.id, e.event.currentTarget.value);
                        },
                        'html': optionsHtml
                    }
                ]
            },
        ]
    }

    // clear content
    document.getElementById('zoneList').textContent = '';

    // NOTE: json2html requires jquery to insert event handlers
    $('#zoneList').json2html(zones, template);

    zones.forEach((zone) => {
        let el = document.getElementById(`select_zone_${zone.id}`);
        el.value = zone.mode;
    });
}

/*******************************************************************************/

async function apiGetPlanningListJson() {
    return await getUrlJson('/ofp-api/v1/plannings');
}

async function initPlanningCreate() {
    let b = document.getElementById('planningCreateButton');
    b.onclick = async function () {
        let el = document.getElementById('newPlanningInput');
        let name = el.value.trim();
        if (name.length === 0) return;
        await createPlanning(name);
        el.value = '';
    }
}

async function createPlanning(name) {
    console.log('createPlanning', name);
    postUrlJson(`/ofp-api/v1/plannings`, { name: name }).catch(logError);
    loadPlanningList().catch(logError);
}

async function renamePlanning(id) {
    console.log('renamePlanning', id);
    let name = promptNonEmptyString('Entrez le nouveau nom du planning');
    patchUrlJson(`/ofp-api/v1/plannings/${id}`, { name: name }).catch(logError);
    loadPlanningList().catch(logError);
}

async function deletePlanning(id) {
    console.log('deletePlanning', id);
    deleteUrl(`/ofp-api/v1/plannings/${id}`).catch(logError);
    loadPlanningList().catch(logError);
}

function getSelectedPlanning() {
    let el = document.getElementById('planningSelect');
    return el.value;
}

async function loadPlanningList() {
    let { plannings } = await apiGetPlanningListJson();

    let template = { '<>': 'option', 'value': '${id}', 'html': '${name}' };

    let el = document.getElementById('planningSelect');
    el.innerHTML = json2html.render(plannings, template);
    el.onchange = async function (e) {
        // action after initial loading : ignore cache for fresh data
        await loadPlanningSlots(this.value, true);
    };

    el = document.getElementById('planningRenameButton');
    el.onclick = async function (e) {
        let el = document.getElementById('planningSelect');
        await renamePlanning(el.value);
    };

    el = document.getElementById('planningDeleteButton');
    el.onclick = async function (e) {
        let el = document.getElementById('planningSelect');
        await deletePlanning(el.value);
    };

    let planningId = getSelectedPlanning();
    if (planningId) {
        // action during initial loading : use cache if possible
        await loadPlanningSlots(planningId);
    }
}

/*******************************************************************************/

async function apiGetPlanningSlotsJson(planningId) {
    return await getUrlJson(`/ofp-api/v1/plannings/${planningId}`);
}

async function changePlanningSlotMode(planningId, startId, newMode) {
    console.log('changePlanningSlotMode', planningId, startId, newMode);
    putUrlJson(`/ofp-api/v1/plannings/${planningId}/slots/${startId}`, { mode: newMode }).catch(logError);
    loadPlanningSlots(planningId).catch(logError);
}

async function changePlanningSlotStart(planningId, startId, newStart) {
    console.log('changePlanningSlotStart', planningId, startId, newStart);
    putUrlJson(`/ofp-api/v1/plannings/${planningId}/slots/${startId}`, { start: newStart }).catch(logError);
    loadPlanningSlots(planningId).catch(logError);
}

async function deletePlanningSlot(planningId, startId) {
    console.log('deletePlanningSlot', planningId, startId);
    deleteUrl(`/ofp-api/v1/plannings/${planningId}/slots/${startId}`).catch(logError);
    loadPlanningSlots(planningId).catch(logError);
}

async function addPlanningSlot(planningId, startId, order) {
    console.log('addPlanningSlot', planningId, startId, order);
    postUrlJson(`/ofp-api/v1/plannings/${planningId}/slots`, { start: startId, mode: order }).catch(logError);
    loadPlanningSlots(planningId).catch(logError);
}

async function loadPlanningSlots(planningId) {
    let { slots } = await apiGetPlanningSlotsJson(planningId);
    let { orders } = await apiGetOrderTypesJson();

    let optionsHtml = json2html.render(orders, { '<>': 'option', 'value': ':fixed:${id}', 'html': '${name}' });

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
                            changePlanningSlotStart(planningId, e.obj.start, e.event.currentTarget.value);
                        }
                    },
                ]
            },
            {
                '<>': 'div', 'class': 'col', 'html': [
                    {
                        '<>': 'select',
                        'id': 'planningSlotSelect_${start}',
                        'class': 'form-select',
                        'onchange': function (e) {
                            changePlanningSlotMode(planningId, e.obj.start, e.event.currentTarget.value);
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
                            deletePlanningSlot(planningId, e.obj.start);
                        },
                        'html': [{ '<>': 'span', 'class': 'bi bi-trash' }]
                    }
                ]
            }
        ]
    };

    // clear content
    document.getElementById('planningSlots').textContent = '';

    // NOTE: json2html requires jquery to insert event handlers
    $('#planningSlots').json2html(slots, template);

    slots.forEach(function (slot) {
        let e = document.getElementById(`planningSlotSelect_${slot.start}`);
        e.value = `:fixed:${slot.order}`;
    });

    // setup new slot UI
    let eli = document.getElementById('newSlotInput');
    let els = document.getElementById('newSlotSelect');
    els.innerHTML = optionsHtml;
    el = document.getElementById('newSlotButton');
    el.onclick = function (e) {
        let start = eli.value.trim();
        if (start.length === 0) return;
        let order = els.value.trim();
        if (order.length === 0) return;
        eli.value = '';
        addPlanningSlot(planningId, start, order);
    };
}

/**************************************************************************/

async function accountCreate(userId, cleartextPassword) {
    console.log('accountCreate', userId, cleartextPassword);
    userId = userId.trim();
    if (userId.length === 0) return null;
    cleartextPassword = userId.trim();
    if (cleartextPassword.length === 0) return null;
    postUrlJson('/ofp-api/v1/accounts', { id: userId, password: cleartextPassword }).catch(logError);
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
    deleteUrl(`/ofp-api/v1/accounts/${userId}`).catch(logError);
    loadAccounts().catch(logError);
}

async function accountPasswordReset(userId) {
    console.log('accountPasswordReset', userId);
    let password = promptNonEmptyString(`Entrez le nouveu password du compte '${userId}'`);
    patchUrlJson(`/ofp-api/v1/accounts/${userId}`, { password: password }).catch(logError);
}

async function loadAccounts() {
    let { accounts } = await apiGetAccountsJson();

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

async function apiGetAccountsJson() {
    return await getUrlJson('/ofp-api/v1/accounts');
}

/*******************************************************************************/

async function uploadFirmware(file) {
    console.log("uploadFirmware", file);
    let formData = new FormData();
    formData.set('file', file);
    postUrl('/ofp-api/v1/upgrade', formData).catch(logError);
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

async function apiGetHardwareTypesJson() {
    return await getUrlJson('/ofp-api/v1/hardware');
}

async function apiGetHardwareParamsJson(hardwareId) {
    return await getUrlJson(`/ofp-api/v1/hardware/${hardwareId}/parameters`);
}

async function loadHardwareSupported() {
    let { current, supported } = await apiGetHardwareTypesJson();

    let template = { '<>': 'option', 'value': '${id}', 'html': '${id}: ${description}' };

    let el = document.getElementById('hardwareSupportedSelect');
    el.innerHTML = json2html.render(supported, template);
    el.onchange = function (e) {
        // action after initial loading : ignore cache for fresh data
        loadHardwareParameters(this.value);
    };

    el.value = current;
    loadHardwareParameters(current);
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

    let el = document.getElementById('hardwareCurrentParameters');

    if (hardwareId === null) {
        el.textContent = '';
        return;
    }

    let { parameters } = await apiGetHardwareParamsJson(hardwareId);

    let template = {
        '<>': 'div', 'class': 'form-floating mb-3', 'html': [
            { '<>': 'input', 'type': '${type}', 'class': 'form-control', 'id': 'hardware_parameter_id_${id}', 'name': '${id}', 'value': '${value}' },
            { '<>': 'label', 'for': 'hardware_parameter_id_${id}', 'html': '${description}' }
        ]
    };

    el.innerHTML = json2html.render(parameters, template);
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
