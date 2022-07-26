
function updateStatus(id_element, id_status, status_message = null, status_bg_class = null) {
    if (status_message === null) {
        // ok
        let el = document.getElementById(id_status);
        el.className = 'invisible';
        el.innerHTML = '';

        el = document.getElementById(id_element);
        el.classList.remove('invisible');
    }
    else {
        // fail
        let el = document.getElementById(id_status);
        el.className = 'md-3 p-3';
        el.classList.add(status_bg_class)
        el.innerHTML = status_message

        el = document.getElementById(id_element);
        el.classList.add('invisible');
    }
}

function handleHttpErrors(response) {
    if (!response.ok) {
        throw Error(response.status);
    }
    return response;
}

function onload_hardware_types() {
    fetch('/samples/hardware_types.json')
        .then(handleHttpErrors)
        .then(res => res.json())
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
    // console.log("onload_hardware");
    onload_hardware_types();
    onload_hardware_parameters();
}

function onload_accounts() {
    console.log("onload_accounts");
    fetch('/samples/accounts.json')
        .then(handleHttpErrors)
        .then(res => res.json())
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
    console.log("account_reset", account_name);
}

// TODO popup + delete
function account_delete(account_name) {
    console.log("account_delete", account_name);
}

function planning_rename() {
    let select = document.getElementById('planningSelect');
    var value = select.options[select.selectedIndex].value;
    console.log("planning_rename", value);
    // TODO: rename (or change with post button and text input... ?)
}

function onload_planning_list() {
    console.log("onload_planning_list");
    fetch('/samples/planningList.json')
        .then(handleHttpErrors)
        .then(res => res.json())
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
    console.log("onload_planning_definition");
}

function onload_planning() {
    console.log("onload_planning");
    onload_planning_list();
    onload_planning_definition();
}

function onload_zones_override() {
    console.log("onload_zone_override");
    fetch('/samples/zones_override.json')
        .then(handleHttpErrors)
        .then(res => res.json())
        .then(function (json) {
            let id = json.override;
            let el = document.getElementById(`oz_${id}`);
            el.checked = true;
            updateStatus('zoneOverride', 'statusZoneOverride');
        })
        .catch(function (err) {
            updateStatus('zoneOverride', 'statusZoneOverride',
                `Impossible de r&eacute;cup&eacute;rer les for&ccedil;ages (${err})`, 'bg-warning');
        });
}

function onload_zones_config() {
    console.log("onload_zones_config");
}

function onload_zones() {
    console.log("onload_zones");
    onload_zones_override();
    onload_zones_config();
}

function onload_hardware_parameters() {
    console.log("onload_hardware_parameters");
}

function onload_body() {
    console.log("onload_body");
    onload_hardware();
    onload_accounts();
    onload_planning();
    onload_zones();
}

