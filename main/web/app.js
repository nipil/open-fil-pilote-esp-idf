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
    fetch('/samples/hardware_types.json')
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
    fetch('/samples/accounts.json')
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

function planning_rename() {
    let select = document.getElementById('planningSelect');
    var value = select.options[select.selectedIndex].value;
    // TODO: rename (or change with post button and text input... ?)
}

function onload_planning_list() {
    fetch('/samples/planningList.json')
        .then(handleHttpErrors)
        .then((res) => res.json())
        .then(function (json) {
            let template = { '<>': 'option', 'value': '${id}', 'html': '${name}' };
            let el = document.getElementById('planningSelect');
            el.innerHTML = json2html.render(json.plannings, template);
            updateStatus('planningListDiv', 'statusPlanningList');
        })
        .catch(function (err) {
            updateStatus('planningListDiv', 'statusPlanningList',
                `Impossible de r&eacute;cup&eacute;rer les plannings (${err})`, 'bg-warning');
        });
}

function onload_planning_definition() {
}

function onload_planning() {
    onload_planning_list();
    onload_planning_definition();
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
}

async function getOrderTypes(reload = false) {
    return getUrl('/samples/orders.json', reload);
}

async function getZoneOverride(reload = false) {
    return getUrl('/samples/zones_override.json', reload);
}

async function loadZoneOverrides(reload = false) {
    let zoneOverrideResponse = await getZoneOverride(reload);
    let zoneOverrideJson = await zoneOverrideResponse.json();
    const zoneOverrideId = zoneOverrideJson.override;

    let orderTypesResponse = await getOrderTypes(reload);
    let orderTypesJson = await orderTypesResponse.json();
    const noOverride = { id: 'none', name: 'Aucun for&ccedil;age', class: 'primary' }
    const overrideTypesJsonAll = [noOverride, ...orderTypesJson.orders];

    let templateLabel = {
        '<>': 'label',
        'class': 'btn btn-outline-${class} w-100',
        'for': 'oz_${id}',
        'onclick': function (el) {
            changeZoneOverrides(el.obj.id);
        },
        'html': '${name}'
    };

    let templateInput = {
        '<>': 'input',
        'type': 'radio',
        'class': 'btn-check w-100',
        'name': 'overrideZones',
        'id': 'oz_${id}',
        'autocomplete': 'off'
    };

    let templateInputChecked = { ...templateInput, 'checked': true };

    let templateMaster = {
        '<>': 'div', 'class': 'col-sm-auto mb-3', 'html': function (element) {
            return $.json2html(this, [
                (this.id == zoneOverrideId ? templateInputChecked : templateInput),
                templateLabel
            ]);
        }
    };

    $('#zoneOverride').json2html(overrideTypesJsonAll, templateMaster);
}

async function getPlanningList(reload = false) {
    return getUrl('/samples/plannings.json', reload);
}

async function changeZoneDescription(zoneId) {
    console.log("changeZoneDescription", zoneId);
}

async function changeZoneMode(zoneId, mode) {
    console.log("changeZoneMode", zoneId, mode);
}

async function changeZoneValue(zoneId, value) {
    console.log("changeZoneValue", zoneId, value);
}

async function getZoneConfig(reload = false) {
    return getUrl('/samples/zones.json', reload);
}

async function loadZoneConfiguration(reload = false) {
    let zoneConfigResponse = await getZoneConfig(reload);
    let zoneConfigJson = await zoneConfigResponse.json();
    const zoneConfig = zoneConfigJson.zones;

    let orderTypesResponse = await getOrderTypes(reload);
    let orderTypesJson = await orderTypesResponse.json();
    const orderTypes = orderTypesJson.orders;

    let planningListResponse = await getPlanningList(reload);
    let planningListJson = await planningListResponse.json();
    const planningList = planningListJson.plannings;

    let optionsHtml = json2html.render(orderTypes, { '<>': 'option', 'value': ':fixed:${id}', 'html': 'Fixe: ${name}' })
        + json2html.render(planningList, { '<>': 'option', 'value': ':planning:${id}', 'html': 'Planning: ${name}' });

    let templateMaster = {
        '<>': 'div', 'class': 'row mb-3', 'html': [
            {
                '<>': 'div', 'class': 'col mb-3', 'html': '(${id}) ${desc}'
            },
            {
                '<>': 'div', 'class': 'col-auto mb-3', 'html': [
                    {
                        '<>': 'button',
                        'class': 'btn btn-warning btn',
                        'onclick': function (el) {
                            changeZoneDescription(el.obj.id);
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
                        'onchange': function (el) {
                            changeZoneValue(el.obj.id, el.event.currentTarget.value);
                        },
                        'html': optionsHtml
                    }
                ]
            },
        ]
    }

    $('#zoneList').json2html(zoneConfig, templateMaster);
}

async function ofp_init() {
    Promise.all([
        loadZoneOverrides(),
        loadZoneConfiguration()
    ]).catch(logError);
}

window.onload = ofp_init

