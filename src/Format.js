.pragma library

// Timecodes always show hours: 00:04:07.
function time(seconds) {
    var s = isFinite(seconds) && seconds > 0 ? Math.floor(seconds) : 0;
    return pad(Math.floor(s / 3600)) + ":" + pad(Math.floor(s / 60) % 60) + ":" + pad(s % 60);
}

// With milliseconds, for frame stepping.
function precise(seconds) {
    var value = isFinite(seconds) && seconds > 0 ? seconds : 0;
    var ms = Math.floor((value - Math.floor(value)) * 1000);
    return time(value) + "." + (ms < 10 ? "00" : ms < 100 ? "0" : "") + ms;
}

function pad(n) {
    return n < 10 ? "0" + n : "" + n;
}

function fileName(path) {
    var s = String(path);
    var slash = s.lastIndexOf("/");
    return slash >= 0 && slash < s.length - 1 ? s.substring(slash + 1) : s;
}
