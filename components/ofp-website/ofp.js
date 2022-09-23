/*******************************************************************************/

const slot_dow = [
    {
        "id": 0,
        "name": "Dimanche",
    },
    {
        "id": 1,
        "name": "Lundi",
    },
    {
        "id": 2,
        "name": "Mardi",
    },
    {
        "id": 3,
        "name": "Mercredi",
    },
    {
        "id": 4,
        "name": "Jeudi",
    },
    {
        "id": 5,
        "name": "Vendredi",
    },
    {
        "id": 6,
        "name": "Samedi",
    },
];

function arrayDictNumber(count) {
    let data = [...Array(count).keys()];
    let expand = data.map((x) => ({ id: x, name: x.toString().padStart(2, '0') }));
    return expand
}

const slot_hours = arrayDictNumber(24);

const slot_minutes = arrayDictNumber(60);

/*******************************************************************************/

function handleHttpErrors(response) {
    if (!response.ok) {
        throw Error(response.status);
    }
    return response;
}

/*******************************************************************************/

async function doUrl(url, errorMessage, options = {}) {
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
    let { user, uptime, firmware } = await apiGetStatusJson();

    let el = document.getElementById('status');

    let d = document.createElement('div');
    d.innerHTML = `Syst\u00E8me d\u00E9mar\u00E9 depuis ${secondsToDuration(uptime.system)}`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `Connect\u00E9 au Wifi depuis ${secondsToDuration(uptime.wifi.current_uptime)} (${Math.floor(100 * uptime.wifi.cumulated_uptime / uptime.system)}%)`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `${uptime.wifi.successes} connexions r\u00E9ussies pour ${uptime.wifi.attempts} tentatives (${Math.floor(100 * uptime.wifi.successes / uptime.wifi.attempts)}%)`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `${uptime.wifi.disconnects} d\u00E9connexions détectées depuis le dernier d\u00E9marrage`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `Le système a d\u00E9marr\u00E9 sur la partition "${firmware.running_partition}" dont la taille maximale est de ${firmware.running_partition_size} octets`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `Microgiciel actif est "${firmware.running_app_name}" en version ${firmware.running_app_version}`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `Microgiciel g\u00E9n\u00E9r\u00E9 le ${firmware.running_app_compiled_date} ${firmware.running_app_compiled_time} \u00E0 l'aide du framework ESP-IDF ${firmware.running_app_idf_version}`;
    el.appendChild(d);

    d = document.createElement('div');
    d.innerText = `L'utilisateur "${user.id}" est connect\u00E9, et son IP apparente est ${user.source_ip}`;
    el.appendChild(d);
}

/*******************************************************************************/

async function changeZoneOverrides(override) {
    console.log("changeZoneOverrides", override);
    await putUrlJson('/ofp-api/v1/override', { override: override }).catch(logError);
    await loadZoneOverrides().catch(logError);
}

async function apiGetOrderTypesJson() {
    // TODO: reorder
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
    await patchUrlJson(`/ofp-api/v1/zones/${zoneId}`, { description: name }).catch(logError);
    await loadZoneConfiguration().catch(logError);
}

async function changeZoneValue(zoneId, value) {
    console.log("changeZoneValue", zoneId, value);
    await patchUrlJson(`/ofp-api/v1/zones/${zoneId}`, { mode: value }).catch(logError);
    await loadZoneConfiguration().catch(logError);
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
                '<>': 'div', 'class': 'col mb-3', 'html': '<b>${description}</b> (${id})'
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
    await postUrlJson(`/ofp-api/v1/plannings`, { name: name }).catch(logError);
    await loadPlanningList().catch(logError);

    // reload zone in case planning was referenced in a zone and zone was updated
    await loadZoneConfiguration().catch(logError);

    // TODO: select newly created planning
}

async function renamePlanning(id) {
    console.log('renamePlanning', id);
    let name = promptNonEmptyString('Entrez le nouveau nom du planning');
    await patchUrlJson(`/ofp-api/v1/plannings/${id}`, { name: name }).catch(logError);
    await loadPlanningList().catch(logError);

    // reload zone to update planning name
    await loadZoneConfiguration().catch(logError);
}

async function deletePlanning(id) {
    console.log('deletePlanning', id);
    await deleteUrl(`/ofp-api/v1/plannings/${id}`).catch(logError);
    await loadPlanningList().catch(logError);

    // reload zone in case planning was referenced in a zone and zone was updated
    await loadZoneConfiguration().catch(logError);
}

function getSelectedPlanning() {
    let el = document.getElementById('planningSelect');
    return el.value;
}

async function loadPlanningList() {
    let { plannings } = await apiGetPlanningListJson();
    plannings.sort((a, b) => a.name - b.name);

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
    } else {
        // clear content
        document.getElementById('planningSlots').innerHTML = `En l'absence de cr&eacute;neaux, l'ordre par d&eacute;faut (confort) est utilis&eacute;.`;
        document.getElementById('planningSlotAddDiv').style.display = 'none';
    }
}

/*******************************************************************************/

async function apiGetPlanningSlotsJson(planningId) {
    return await getUrlJson(`/ofp-api/v1/plannings/${planningId}`);
}

async function changePlanningSlotDay(planningId, slotId, newDow) {
    console.log('changePlanningSlotDay', planningId, slotId, newDow);
    await patchUrlJson(`/ofp-api/v1/plannings/${planningId}/slots/${slotId}`, { dow: +newDow }).catch(logError);
    await loadPlanningSlots(planningId).catch(logError);
}

async function changePlanningSlotHour(planningId, slotId, newHour) {
    console.log('changePlanningSlotHour', planningId, slotId, newHour);
    await patchUrlJson(`/ofp-api/v1/plannings/${planningId}/slots/${slotId}`, { hour: +newHour }).catch(logError);
    await loadPlanningSlots(planningId).catch(logError);
}

async function changePlanningSlotMinute(planningId, slotId, newMinute) {
    console.log('changePlanningSlotMinute', planningId, slotId, newMinute);
    await patchUrlJson(`/ofp-api/v1/plannings/${planningId}/slots/${slotId}`, { minute: +newMinute }).catch(logError);
    await loadPlanningSlots(planningId).catch(logError);
}

async function changePlanningSlotMode(planningId, slotId, newOrder) {
    console.log('changePlanningSlotMode', planningId, slotId, newOrder);
    await patchUrlJson(`/ofp-api/v1/plannings/${planningId}/slots/${slotId}`, { order: newOrder }).catch(logError);
    await loadPlanningSlots(planningId).catch(logError);
}

async function deletePlanningSlot(planningId, slotId) {
    console.log('deletePlanningSlot', planningId, slotId);
    await deleteUrl(`/ofp-api/v1/plannings/${planningId}/slots/${slotId}`).catch(logError);
    await loadPlanningSlots(planningId).catch(logError);
}

async function addPlanningSlot(planningId, dow, hour, minute, order) {
    console.log('addPlanningSlot', planningId, dow, hour, minute, order);
    await postUrlJson(`/ofp-api/v1/plannings/${planningId}/slots`, { dow: +dow, hour: +hour, minute: +minute, order: order }).catch(logError);
    await loadPlanningSlots(planningId).catch(logError);
}

async function loadPlanningSlots(planningId) {
    let { slots } = await apiGetPlanningSlotsJson(planningId);

    // order by name
    slots.sort((a, b) => {
        if (a.dow !== b.dow) return (a.dow < b.dow) ? -1 : 1;
        if (a.hour !== b.hour) return (a.hour < b.hour) ? -1 : 1;
        if (a.minute !== b.minute) return (a.minute < b.minute) ? -1 : 1;
        return 0;
    });

    let { orders } = await apiGetOrderTypesJson();

    let dowHtml = json2html.render(slot_dow, { '<>': 'option', 'value': '${id}', 'html': '${name}' });
    let hourHtml = json2html.render(slot_hours, { '<>': 'option', 'value': '${id}', 'html': '${name}h' });
    let minuteHtml = json2html.render(slot_minutes, { '<>': 'option', 'value': '${id}', 'html': '${name}m' });
    let optionsHtml = json2html.render(orders, { '<>': 'option', 'value': '${id}', 'html': '${name}' });

    let template = {
        '<>': 'div', 'class': 'row mb-3', 'html': [
            {
                '<>': 'div', 'class': 'col-sm', 'html': [
                    {
                        '<>': 'select',
                        'id': 'planningSlotSelectDow_${id}',
                        'class': 'form-select',
                        'onchange': function (e) {
                            changePlanningSlotDay(planningId, e.obj.id, e.event.currentTarget.value);
                        },
                        'html': dowHtml
                    }
                ]
            },
            {
                '<>': 'div', 'class': 'col-sm', 'html': [
                    {
                        '<>': 'select',
                        'id': 'planningSlotSelectHour_${id}',
                        'class': 'form-select',
                        'onchange': function (e) {
                            changePlanningSlotHour(planningId, e.obj.id, e.event.currentTarget.value);
                        },
                        'html': hourHtml
                    }
                ]
            },
            {
                '<>': 'div', 'class': 'col-sm', 'html': [
                    {
                        '<>': 'select',
                        'id': 'planningSlotSelectMinute_${id}',
                        'class': 'form-select',
                        'onchange': function (e) {
                            changePlanningSlotMinute(planningId, e.obj.id, e.event.currentTarget.value);
                        },
                        'html': minuteHtml
                    }
                ]
            },
            {
                '<>': 'div', 'class': 'col-sm', 'html': [
                    {
                        '<>': 'select',
                        'id': 'planningSlotSelectOrder_${id}',
                        'class': 'form-select',
                        'onchange': function (e) {
                            changePlanningSlotMode(planningId, e.obj.id, e.event.currentTarget.value);
                        },
                        'html': optionsHtml
                    }
                ]
            },
            {
                '<>': 'div', 'class': 'col-sm', 'html': [
                    {
                        '<>': 'button', 'class': 'btn btn-danger',
                        'onclick': function (e) {
                            deletePlanningSlot(planningId, e.obj.id);
                        },
                        'html': [{ '<>': 'span', 'class': 'bi bi-trash' }]
                    }
                ]
            }
        ]
    };

    // clear content
    document.getElementById('planningSlots').textContent = '';
    document.getElementById('planningSlotAddDiv').style.display = '';

    // NOTE: json2html requires jquery to insert event handlers
    $('#planningSlots').json2html(slots, template);

    // select current data
    slots.forEach(function (slot) {
        let e = document.getElementById(`planningSlotSelectDow_${slot.id}`);
        e.value = slot.dow;
        e = document.getElementById(`planningSlotSelectHour_${slot.id}`);
        e.value = slot.hour;
        e = document.getElementById(`planningSlotSelectMinute_${slot.id}`);
        e.value = slot.minute;
        e = document.getElementById(`planningSlotSelectOrder_${slot.id}`);
        e.value = slot.order;
    });

    // add inputs
    let eld = document.getElementById('planningSlotSelectDow_add');
    eld.innerHTML = dowHtml;
    let elh = document.getElementById('planningSlotSelectHour_add');
    elh.innerHTML = hourHtml;
    let elm = document.getElementById('planningSlotSelectMinute_add');
    elm.innerHTML = minuteHtml;
    let elo = document.getElementById('planningSlotSelectOrder_add');
    elo.innerHTML = optionsHtml;

    // add button
    el = document.getElementById('newSlotButton');
    el.onclick = async function (e) {
        addPlanningSlot(planningId, eld.value, elh.value, elm.value, elo.value);
        eld.value = null;
        elh.value = null;
        elm.value = null;
        elo.value = null;
    }
}

/**************************************************************************/

async function accountCreate(userId, cleartextPassword) {
    console.log('accountCreate', userId, cleartextPassword);
    userId = userId.trim();
    if (userId.length === 0) return null;
    cleartextPassword = cleartextPassword.trim();
    if (cleartextPassword.length === 0) return null;
    await postUrlJson('/ofp-api/v1/accounts', { id: userId, password: cleartextPassword }).catch(logError);
    await loadAccounts().catch(logError);
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
    await deleteUrl(`/ofp-api/v1/accounts/${userId}`).catch(logError);
    await loadAccounts().catch(logError);
}

async function accountPasswordReset(userId) {
    console.log('accountPasswordReset', userId);
    let password = promptNonEmptyString(`Entrez le nouveu password du compte '${userId}'`);
    await patchUrlJson(`/ofp-api/v1/accounts/${userId}`, { password: password }).catch(logError);
}

function accountCheckAdmin(accounts) {
    let userIsAdmin = false;
    for (const account of accounts) {
        if (account.id !== "admin")
            continue;
        userIsAdmin = true;
        break;
    }

    if (!userIsAdmin) {
        document.getElementById('accountCreationDiv').style.display = 'none';
        document.getElementById('headingFirmwareDiv').style.display = 'none';
        document.getElementById('headingCertificateDiv').style.display = 'none';
        document.getElementById('headingHardwareDiv').style.display = 'none';
    }
}

async function loadAccounts() {
    let { accounts } = await apiGetAccountsJson();

    accountCheckAdmin(accounts);

    let template = {
        '<>': 'tr', 'html': [
            { '<>': 'th', 'html': '${id}' },
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
    el.innerHTML = '<tr><th>Nom</th><th>MdP</th><th>Suppr.</th></tr>';

    // NOTE: json2html requires jquery to insert event handlers
    $('#accountListTable').json2html(accounts, template);
}

async function apiGetAccountsJson() {
    return await getUrlJson('/ofp-api/v1/accounts');
}

/*******************************************************************************/

async function uploadFirmware(file) {
    console.log("uploadFirmware", file);
    let options = {
        headers: {
            'Content-Type': 'application/octet-stream'
        }
    };
    let res = await postUrl('/ofp-api/v1/upgrade', file, options).catch(logError);
    if (res.status == 200) {
        window.location.href = '/ofp-api/v1/reboot';
    }
}

async function initFirmwareButtons() {
    el = document.getElementById('updateUploadButton');
    el.onclick = function (e) {
        let t = document.getElementById('updateTextFilePath');
        if (t.files.length != 1) return;
        let status = confirm('Etes vous certain de vouloir charger un nouveau microgiciel ? La centrale devra redémarrer pour prendre en compte le changement.');
        if (status !== true)
            return;
        uploadFirmware(t.files[0]);
    }
}

/*******************************************************************************/

async function uploadCertificate(file) {
    console.log("uploadCertificate", file);
    let options = {
        headers: {
            'Content-Type': 'appliapplication/x-pem-file'
        }
    };
    let res = await postUrl('/ofp-api/v1/certificate', file, options).catch(logError);
    if (res.status == 200) {
        window.location.href = '/ofp-api/v1/reboot';
    }
}

async function initCertificateButtons() {
    el = document.getElementById('updateCertificateButton');
    el.onclick = function (e) {
        let t = document.getElementById('certificateTextFilePath');
        if (t.files.length != 1) return;
        let status = confirm('Etes vous certain de vouloir charger un nouveau bundle de certificats ? La centrale devra redémarrer pour prendre en compte le changement.');
        if (status !== true)
            return;
        uploadCertificate(t.files[0]);
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

let isIntervalInProgress = false;

async function ofp_init() {
    await loadStatus().catch(logError);
    await loadZoneOverrides().catch(logError);
    await loadZoneConfiguration().catch(logError);
    await initPlanningCreate().catch(logError);
    await loadPlanningList().catch(logError);
    await initAccountCreate().catch(logError);
    await loadAccounts().catch(logError);
    await initFirmwareButtons().catch(logError);
    await initCertificateButtons().catch(logError);
    await loadHardwareSupported().catch(logError);
    await initHardwareParametersButtons().catch(logError);

    // periodically refresh zone
    setInterval(function () {
        if (isIntervalInProgress)
            return false;
        isIntervalInProgress = true;
        loadZoneConfiguration();
        isIntervalInProgress = false;
    }, 5000);
}

window.onload = ofp_init
