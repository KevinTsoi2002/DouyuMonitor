.pragma library

function densityRatio(density) {
    if (density === "massive") return 1.0
    if (density === "reduced") return 0.4
    return 0.7
}

function lanes(height, fontSize, region, density) {
    const lineHeight = Math.max(1, fontSize * 1.35)
    const regionHeight = region === "full" ? height : height / 2
    const regionTop = region === "bottom" ? height / 2 : 0
    const maximum = Math.max(1, Math.floor(regionHeight / lineHeight))
    const count = Math.max(1, Math.floor(maximum * densityRatio(density)))
    const result = []

    for (let index = 0; index < count; ++index) {
        result.push({ index: index, top: regionTop + index * lineHeight })
    }
    return result
}

function canReuse(active, candidate) {
    const elapsed = Math.max(0, Date.now() - active.launchedAt)
    if (elapsed >= active.durationMs) return true

    const activeDistance = active.containerWidth + active.width
    const activeX = active.containerWidth - activeDistance * elapsed / active.durationMs
    const headGap = candidate.containerWidth - (activeX + active.width)
    if (headGap <= 0) return false

    const candidateSpeed = (candidate.containerWidth + candidate.width) / candidate.durationMs
    const activeSpeed = activeDistance / active.durationMs
    if (candidateSpeed <= activeSpeed) return true

    const catchUpMs = headGap / (candidateSpeed - activeSpeed)
    return catchUpMs >= active.durationMs - elapsed
}

function selectLane(laneList, activeItems, candidate) {
    for (let laneIndex = 0; laneIndex < laneList.length; ++laneIndex) {
        const lane = laneList[laneIndex]
        let safe = true
        for (let activeIndex = 0; activeIndex < activeItems.length; ++activeIndex) {
            const active = activeItems[activeIndex]
            if (active.laneIndex !== lane.index) continue
            if (!canReuse(active, candidate)) {
                safe = false
                break
            }
        }
        if (safe) return lane
    }
    return null
}
