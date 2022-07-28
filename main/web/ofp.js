function updateStatus(id_element, id_status, status_message = null, status_bg_class = null) {
    if (status_message === null) {
        // ok
        let el = document.getElementById(id_status);
        el.className = 'invisible';
        el.innerHTML = '';

        if (id_element) {
            el = document.getElementById(id_element);
            el.classList.remove('invisible');
        }
    }
    else {
        // fail
        let el = document.getElementById(id_status);
        el.className = 'md-3 p-3';
        el.classList.add(status_bg_class)
        el.innerHTML = status_message

        if (id_element) {
            el = document.getElementById(id_element);
            el.classList.add('invisible');
        }
    }
}

/*******************************************************************************/

function handleHttpErrors(response) {
    if (!response.ok) {
        throw Error(response.status);
    }
    return response;
}

function getUrl(url, reload = false) {
    let options = {}
    if (reload) {
        options = { cache: "reload" }
    }
    return fetch(url, options)
        .then(handleHttpErrors);
}

/*******************************************************************************/

function onload_hardware_types() {
    fetch('samples/hardware_types.json')
        .then(handleHttpErrors)
        .then((res) => res.json())
        .then(function (json) {
            // to enable selection of current hardware
            let current_type_id = json.current_type_id;
            // do a shallow clone and append 1 argument
            let sub_template_short = { '<>': 'option', 'value': '${id}', 'html': '${id}: ${desc}' };
            let sub_template_full = { ...sub_template_short };
            sub_template_full.selected = true;
            // special template because of optional argument
            let template = {
                'html': function () {
                    return json2html.render(this, this.id == current_type_id ? sub_template_full : sub_template_short);
                }
            };
            let element = document.getElementById('hardwareTypeSelect');
            element.innerHTML = json2html.render(json.supported_types, template);

            updateStatus('hardwareSupported', 'statusHardwareSupported');
        })
        .catch(function (err) {
            updateStatus('hardwareSupported', 'statusHardwareSupported',
                `Impossible de r&eacute;cup&eacute;rer les types de mat&eacute;riel support&eacute;s (${err})`, 'bg-warning');
        });
}

function onload_hardware() {
    onload_hardware_types();
    onload_hardware_parameters();
}

function onload_accounts() {
    fetch('samples/accounts.json')
        .then(handleHttpErrors)
        .then((res) => res.json())
        .then(function (json) {
            let template = {
                '<>': 'tr', 'html': [
                    { '<>': 'th', 'html': '${id}' },
                    { '<>': 'td', 'html': '${type}' },
                    {
                        '<>': 'td', 'html': [
                            { '<>': 'button', 'class': 'btn btn-primary', 'onclick': function (e) { account_reset(e.obj.id); }, 'html': 'R&eacute;initialiser' }, ,
                        ]
                    },
                    {
                        '<>': 'td', 'html': [
                            { '<>': 'button', 'class': 'btn btn-danger', 'onclick': function (e) { account_delete(e.obj.id); }, 'html': 'Supprimer' }, ,
                        ]
                    }
                ]
            };

            // NOTE: json2html requires jquery to insert event handlers
            $('#accountListTable').json2html(json.accounts, template);

            updateStatus('accountList', 'statusAccountList');

        })
        .catch(function (err) {
            updateStatus('accountList', 'statusAccountList',
                `Impossible de r&eacute;cup&eacute;rer les comptes utilisateur (${err})`, 'bg-warning');
        });
}

// TODO popup + post
function account_reset(account_name) {
}

// TODO popup + delete
function account_delete(account_name) {
}


function onload_zones() {
    console.log("onload_zones");
    onload_zone_types();
    onload_zones_override();
    onload_zones_config();
}

function onload_hardware_parameters() {
}

/* ************************************************************************** */

function logError(message) {
    let msg = document.createElement("div");
    let txt = document.createTextNode(message);
    msg.appendChild(txt);

    let el = document.getElementById('status');
    el.appendChild(msg);
    el.className = 'mb-3 p-3 bg-warning';
}

async function changeZoneOverrides(override) {
    console.log("changeZoneOverrides", override);
    // TODO
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

    // NOTE: json2html requires jquery to insert event handlers
    $('#zoneOverride').json2html(overrideTypesJsonAll, templateMaster);

    let el = document.getElementById(`zoneOverride_${zoneOverrideId}`)
    el.toggleAttribute("checked", true);
}

async function apiGetPlanningListJson(reload = false) {
    let planningListResponse = await getUrl('samples/plannings.json', reload);
    let planningListJson = await planningListResponse.json();
    return planningListJson.plannings;
}

async function changeZoneDescription(zoneId) {
    console.log("changeZoneDescription", zoneId);
    // TODO
}

async function changeZoneMode(zoneId, mode) {
    console.log("changeZoneMode", zoneId, mode);
    // TODO
}

async function changeZoneValue(zoneId, value) {
    console.log("changeZoneValue", zoneId, value);
    // TODO
}

async function apiGetZoneConfigJson(reload = false) {
    let zoneConfigResponse = await getUrl('samples/zones.json', reload);
    let zoneConfigJson = await zoneConfigResponse.json();
    return zoneConfigJson.zones;
}

async function loadZoneConfiguration(reload = false) {
    let zoneConfig = await apiGetZoneConfigJson(reload);
    let orderTypes = await apiGetOrderTypesJson(reload);
    let planningList = await apiGetPlanningListJson(reload);

    let optionsHtml = json2html.render(orderTypes, { '<>': 'option', 'value': ':fixed:${id}', 'html': 'Fixe: ${name}' })
        + json2html.render(planningList, { '<>': 'option', 'value': ':planning:${id}', 'html': 'Programmation: ${name}' });

    let template = {
        '<>': 'div', 'class': 'row mb-3', 'html': [
            {
                '<>': 'div', 'class': 'col mb-3', 'html': '${desc} (${id})'
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
        el.value = zone.value;
    });
}

async function initPlanningCreate() {
    let b = document.getElementById('planningCreateButton');
    b.onclick = function (e) {
        let t = document.getElementById('planningCreateEdit');
        createPlanning(t.value);
    }
}


function createPlanning(name)  {
    console.log('createPlanning', name);
    // TODO
}

function renamePlanning(id) {
    console.log('renamePlanning', id);
    // TODO
}

function deletePlanning(id) {
    console.log('deletePlanning', id);
    // TODO
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

async function apiGetPlanningDetailsJson(reload = false) {
    let planningDetailsResponse = await getUrl('samples/planning_details.json', reload);
    let planningDetailsJson = await planningDetailsResponse.json();
    return planningDetailsJson.slots;
}

async function changePlanningDetailOrder(planningId, start, newOrder) {
    console.log('changePlanningDetailOrder', planningId, start, newOrder);
    // TODO
}

async function changePlanningDetailStart(planningId, start, newStart) {
    console.log('changePlanningDetailStart', planningId, start, newStart);
    // TODO
}

async function deletePlanningDetail(planningId, value) {
    console.log('deletePlanningDetail', planningId, value);
    // TODO
}

async function addPlanningDetailSlot(planningId) {
    console.log('addPlanningDetailSlot', planningId);
    // TODO
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
                            changePlanningDetailOrder(planningId, e.obj.start, e.event.currentTarget.value);
                        },
                        'html': optionsHtml
                    }
                ]
            },
            {
                '<>': 'div', 'class': 'col', 'html': [
                    {
                        '<>': 'button', 'class': 'btn btn-danger',
                        'onclick': function (e) {
                            deletePlanningDetail(planningId, e.obj.start);
                        },
                        'html': 'Supprimer'
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

async function ofp_init() {
    Promise.all([
        loadZoneOverrides(),
        loadZoneConfiguration(),
        initPlanningCreate(),
        loadPlanningList()
    ]).catch(logError);
}

window.onload = ofp_init

