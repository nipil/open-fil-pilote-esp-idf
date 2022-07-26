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

function zone_set_mode(zone_id, mode) {
}

function zone_set_value(zone_id, value) {
}

function onload_zones_config() {
    fetch('/samples/zonesConfig.json')
        .then(handleHttpErrors)
        .then((res) => res.json())
        .then(function (json) {
            let template = {
                '<>': 'div', 'class': 'row mb-3', 'html': [
                    { '<>': 'div', 'class': 'col-sm', 'html': '${id}' },
                    { '<>': 'div', 'class': 'col-sm', 'html': '${desc}' },
                    { '<>': 'div', 'class': 'col-sm', 'html': 'rename' },
                    { '<>': 'div', 'class': 'col-sm', 'html': '${mode}' },
                    { '<>': 'div', 'class': 'col-sm', 'html': '${value}' }
                ]
            };
            $('#zoneList').json2html(json.zones, template);
            updateStatus('zoneList', 'statusZoneList');
        })
        .catch(function (err) {
            updateStatus('zoneList', 'statusZoneList',
                `Impossible de r&eacute;cup&eacute;rer la configuration des zones (${err})`, 'bg-warning');
        });
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

async function changeZoneOverrides(newOverride) {
    console.log("changeZoneOverrides", newOverride);
}

function getZoneTypes(reload = false) {
    return getUrl('/samples/zoneTypes.json', reload);
}

function getZoneOverride(reload = false) {
    return getUrl('/samples/zones_override.json', reload);
}

async function loadZoneOverrides() {
    let zoneOverrideResponse = await getZoneOverride();
    let zoneOverrideJson = await zoneOverrideResponse.json();
    const zoneOverrideId = zoneOverrideJson.override;

    let zoneTypesResponse = await getZoneTypes();
    let zoneTypesJson = await zoneTypesResponse.json();
    const none = { id: 'none', name: 'Aucun', class: 'primary' }
    const zoneTypesJsonAll = [none, ...zoneTypesJson.zone_types];

    let templateLabel = {
        '<>': 'label',
        'class': 'btn btn-outline-${class} w-100',
        'for': 'oz_${id}',
        'html': '${name}',
        onclick: function (el) {
            changeZoneOverrides(el.obj.id);
        }
    };

    let templateInput = {
        '<>': 'input',
        'type': 'radio',
        'class': 'btn-check w-100',
        'name': 'overrideZones',
        'id': 'oz_${id}',
        'autocomplete': 'off',
        onclick: function (el) {
        }
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

    $('#zoneOverride').json2html(zoneTypesJsonAll, templateMaster);
}

async function ofp_init() {
    Promise.all([
        loadZoneOverrides()
    ]).catch(logError);

    /*
        onload_hardware();
        onload_accounts();
        onload_planning();
        onload_zones();
    */
}

window.onload = ofp_init

