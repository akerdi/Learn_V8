if (options.verbose) {
  log(options)
}

const map = new Map()
map.set("key0", "value0");
map.set("key1", "value1");
log(map);

log("str log.")

function Process(request) {
  log("verbose: " + options.verbose)
  if (options.verbose) {
    log("Processing " + request.path);
  }
  if (!output[request.path]) {
    output[request.path] = 1;
  } else {
    output[request.path]++
  }
}
