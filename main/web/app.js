
function onload_zones_override() {
    console.log("onload_zone_override");
}

function onload_zones_config() {
    console.log("onload_zones_config");
}

function onload_zones() {
    console.log("onload_zones");
    onload_zones_override();
    onload_zones_config();
}

function onload_planning_list() {
    console.log("onload_planning_list");
}

function onload_planning_definition() {
    console.log("onload_planning_definition");
}

function onload_planning() {
    console.log("onload_planning");
    onload_planning_list();
    onload_planning_definition();
}

function onload_accounts() {
    console.log("onload_accounts");
}

function onload_hardware_parameters() {
    console.log("onload_hardware_parameters");
}

function onload_hardware_types() {
    console.log("onload_hardware_types");
}

function onload_hardware() {
    console.log("onload_hardware");
    onload_hardware_types();
    onload_hardware_parameters();
}

function onload_body() {
    console.log("onload_body");
    onload_hardware();
    onload_accounts();
    onload_planning();
    onload_zones();
}
