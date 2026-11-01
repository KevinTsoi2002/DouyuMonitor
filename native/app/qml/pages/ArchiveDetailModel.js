.pragma library

// Presentation-only extraction from the bundled archive. Scores are never recomputed.
function lines(text) {
    return String(text || "").split(/\r?\n/).map(function(line) { return line.trim(); })
        .filter(function(line) { return line.length > 0; });
}

function isTeam(text) { return /^(红|蓝|黑|紫)队$/.test(text); }
function isHeading(text) { return /^[^\u4e00-\u9fa5A-Za-z0-9←]/.test(text); }
function roundStart(source, index) {
    if (/^第\s*\d+\s*[局轮]$/.test(source[index])) return true;
    return /^第\s*\d+\s*场$/.test(source[index])
        && isTeam(source[index + 1]) && /^vs$/i.test(source[index + 2])
        && isTeam(source[index + 3]);
}

function sectionBlocks(source) {
    var blocks = [], current = null;
    for (var i = 0; i < source.length; ++i) {
        var line = source[i];
        if (isHeading(line) && line.length < 55) {
            current = { title: line, text: "" };
            blocks.push(current);
        } else {
            if (!current) {
                current = { title: "赛制与记录", text: "" };
                blocks.push(current);
            }
            current.text += (current.text ? "\n" : "") + line;
        }
    }
    return blocks;
}

function parseRound(source) {
    var round = { label: source[0], title: "", outcome: "", teams: [], notes: [] };
    var cursor = 1;
    if (isTeam(source[1]) && /^vs$/i.test(source[2])) {
        round.title = source[1] + "  vs  " + source[3];
        cursor = 4;
    } else if (source[1] && !isTeam(source[1])) {
        round.title = source[1];
        cursor = 2;
    }
    while (cursor < source.length) {
        var line = source[cursor];
        if (isTeam(line) && !/^vs$/i.test(source[cursor + 1] || "")) {
            var team = { team: line, value: "", detail: "" }, details = [];
            ++cursor;
            if (/^合计\s/.test(source[cursor] || "")) {
                team.value = source[cursor].replace(/^合计\s*/, "");
                ++cursor;
            }
            while (cursor < source.length && !isTeam(source[cursor])
                   && !/^第\s*\d+\s*场$/.test(source[cursor])
                   && !/^🏆/.test(source[cursor])) details.push(source[cursor++]);
            team.detail = details.join("\n");
            round.teams.push(team);
        } else if (/^🏆/.test(line) || (/队 胜$/.test(line) && !round.outcome)) {
            round.outcome = line;
            ++cursor;
        } else if (/^第\s*\d+\s*场$/.test(line) && /^vs$/i.test(source[cursor + 2] || "")) {
            round.notes.push(line + " · " + source[cursor + 1] + " vs " + source[cursor + 3]
                             + (source[cursor + 4] ? " · " + source[cursor + 4] : ""));
            cursor += 5;
        } else {
            round.notes.push(line);
            ++cursor;
        }
    }
    return round;
}

function season(match) {
    match = match || {};
    var raw = typeof match.detail === "string" ? match.detail : "";
    var source = lines(raw).filter(function(line) {
        return line !== "← 返回总览" && line !== match.date && line !== match.title;
    });
    var result = { title: String(match.title || "赛事详情"), date: String(match.date || ""),
        raw: raw, outcome: "", standings: [], standingsTitle: "已记录成绩", standingsNote: "",
        sections: [], rounds: [] };
    var starts = [];
    for (var i = 0; i < source.length; ++i) if (roundStart(source, i)) starts.push(i);
    var prefix = source.slice(0, starts.length ? starts[0] : source.length);
    var firstRank = -1;
    for (var p = 0; p + 2 < prefix.length; ++p) {
        if (/^[1-4]$/.test(prefix[p]) && isTeam(prefix[p + 1])) { firstRank = p; break; }
    }
    var remainder = prefix.slice();
    var rankingHeading = -1;
    for (var q = firstRank - 1; q >= 0; --q) {
        if (/总积分|各队总分|队伍战况|总名次|命数/.test(prefix[q]) && prefix[q].length < 40) {
            rankingHeading = q;
            break;
        }
    }
    if (firstRank >= 0 && rankingHeading >= 0) {
        var end = firstRank;
        var parsed = [];
        while (end + 2 < prefix.length && /^[1-4]$/.test(prefix[end]) && isTeam(prefix[end + 1]) && parsed.length < 4) {
            var rank = prefix[end++], team = prefix[end++], notes = [];
            while (end < prefix.length && /^(同分|并列)/.test(prefix[end])) notes.push(prefix[end++]);
            while (end < prefix.length && /^❤/.test(prefix[end])) ++end;
            var value = prefix[end++] || "—";
            if (!/^[+-]?\d+(?:\.\d+)?\s*(?:分|胜|圈|命)$|^\d+:\d+:\d+$/.test(value)) break;
            while (end < prefix.length && !(/^[1-4]$/.test(prefix[end]) && isTeam(prefix[end + 1]))
                   && (!isHeading(prefix[end]) || /^👑/.test(prefix[end])) && prefix[end].length < 90)
                notes.push(prefix[end++]);
            parsed.push({ rank: rank, team: team, value: value, note: notes.join(" · ") });
        }
        // Mixed or unrecognized tables stay in the source sections instead of guessing their columns.
        if (parsed.length === 4 && new Set(parsed.map(function(row) { return row.team; })).size === 4) {
            result.standings = parsed;
            remainder = prefix.slice(0, firstRank).concat(prefix.slice(end));
            for (var h = firstRank - 1; h >= 0; --h) {
                if (isHeading(prefix[h])) {
                    result.standingsTitle = prefix[h];
                    result.standingsNote = prefix.slice(h + 1, firstRank).join(" · ");
                    remainder = remainder.filter(function(line) {
                        return line !== result.standingsTitle
                            && prefix.slice(h + 1, firstRank).indexOf(line) < 0;
                    });
                    break;
                }
            }
        }
    }
    remainder = remainder.filter(function(line) {
        if (/^🏆 本场获胜/.test(line)) { result.outcome = line; return false; }
        return true;
    });
    result.sections = sectionBlocks(remainder);
    for (var r = 0; r < starts.length; ++r)
        result.rounds.push(parseRound(source.slice(starts[r], r + 1 < starts.length ? starts[r + 1] : source.length)));
    return result;
}

function football(match) {
    match = match || {};
    var summary = lines(match.summary), detail = match.detail || {}, sections = detail.sections || [];
    var result = { code: summary[0] || "", home: summary[1] || "—", score: summary[2] || "—",
        away: summary[3] || "—", kickoff: (summary[4] || "").replace(/^🕐\s*/, ""),
        outcome: summary.slice(5).join(" · "), date: detail.date || "", note: detail.note || "",
        lineups: [], events: [], votes: [], voteNote: "", eventNote: "", status: "", extra: [],
        raw: [String(match.summary || ""), detail.date || "", detail.title || "", detail.note || ""].concat(sections.map(function(s) { return s.text || ""; })).join("\n\n") };
    for (var s = 0; s < sections.length; ++s) {
        var section = sections[s], source = lines(section.text);
        if (section.kind === "fb-board" && source.length >= 22) {
            var cursor = 6;
            for (var t = 0; t < 2; ++t) {
                var lineup = { team: t === 0 ? result.home : result.away, players: [] };
                for (var j = 0; j < 4; ++j) {
                    lineup.players.push({ position: source[cursor] || "—", name: source[cursor + 1] || "—" });
                    cursor += 2;
                }
                result.lineups.push(lineup);
            }
        } else if (section.kind === "fb-timer") {
            result.status = source[1] || source[0] || "";
        } else if (section.kind === "fb-sec" && source[0].indexOf("事件记录") >= 0) {
            var active = null;
            for (var e = 2; e < source.length; ++e) {
                if (/^\d+(?:\+\d+)?′$/.test(source[e])) {
                    active = { time: source[e], icon: source[e + 1] || "", text: source[e + 2] || "" };
                    result.events.push(active);
                    e += 2;
                } else result.eventNote += (result.eventNote ? "\n" : "") + source[e];
            }
        } else if (section.kind === "fb-mvp-cols") {
            var currentTeam = "";
            for (var v = 0; v < source.length;) {
                if (isTeam(source[v])) { currentTeam = source[v++]; continue; }
                if (/^[门后中锋替]$/.test(source[v]) && /^\d+$/.test(source[v + 2] || "")) {
                    result.votes.push({ team: currentTeam, position: source[v], name: source[v + 1], votes: source[v + 2] });
                    v += 3;
                } else ++v;
            }
        } else if (section.kind === "fb-sec" && source[0].indexOf("MVP") >= 0) {
            result.voteNote = source[1] || "";
        } else result.extra.push({title: source[0] || "附加记录", text: source.slice(1).join("\n")});
    }
    return result;
}

function matchTitle(match, isFootball) {
    if (!match) return "";
    if (!isFootball) return String(match.title || "赛事详情");
    var summary = lines(match.summary);
    return summary.length >= 4 ? summary[0] + " · " + summary[1] + "  " + summary[2] + "  " + summary[3]
                               : String(match.detail && match.detail.title || "比赛详情");
}
