import { Plus, Sparkles } from 'lucide-react';
import { useEffect, useMemo, useRef, useState, type CSSProperties } from 'react';
import {
  calculateLayout,
  calculatePrimaryFocusLayout,
  resolveLayoutId,
  type WorkspaceSize,
} from '../../domain/layout-engine';
import { normalizeRoomPlacementOrder } from '../store/room-placement';
import { useWorkspace } from '../store/workspace-context';
import { PrimaryRoomDivider } from './PrimaryRoomDivider';
import { RoomTile } from './RoomTile';

interface WorkspaceGridProps {
  onAddRoom: () => void;
}

const UNMEASURED_SIZE: WorkspaceSize = { width: 0, height: 0 };

export function WorkspaceGrid({ onAddRoom }: WorkspaceGridProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const layoutId = useWorkspace((state) => state.layoutId);
  const primaryRoomId = useWorkspace((state) => state.primaryRoomId);
  const roomPlacementOrder = useWorkspace((state) => state.roomPlacementOrder);
  const primaryRoomRatio = useWorkspace((state) => state.primaryRoomRatio);
  const setPrimaryRoomRatio = useWorkspace((state) => state.setPrimaryRoomRatio);
  const gridRef = useRef<HTMLDivElement>(null);
  const [workspaceSize, setWorkspaceSize] = useState<WorkspaceSize>(UNMEASURED_SIZE);
  const [previewRatio, setPreviewRatio] = useState<number>();
  const [resizing, setResizing] = useState(false);
  const resolvedLayoutId = resolveLayoutId(layoutId, rooms.length);
  const activeRoomIds = rooms.map((room) => room.roomId);
  const orderedRoomIds = normalizeRoomPlacementOrder(activeRoomIds, roomPlacementOrder);
  const focusPlan = resolvedLayoutId === 'primary-two' && rooms.length > 0
    ? calculatePrimaryFocusLayout(
        orderedRoomIds,
        primaryRoomId,
        primaryRoomRatio,
        workspaceSize,
      )
    : undefined;
  const slots = focusPlan?.slots
    ?? calculateLayout(activeRoomIds, resolvedLayoutId, primaryRoomId);
  const slotsByRoomId = useMemo(
    () => new Map(slots.map((slot) => [slot.roomId, slot])),
    [slots],
  );
  const activeRatio = previewRatio ?? focusPlan?.effectiveRatio ?? primaryRoomRatio;
  const gridStyle = {
    '--room-count': rooms.length,
    '--primary-ratio': `${activeRatio * 100}%`,
    '--secondary-columns': focusPlan?.secondaryColumns ?? 1,
    '--secondary-rows': Math.max(1, focusPlan?.secondaryRows ?? 1),
  } as CSSProperties;

  useEffect(() => {
    const element = gridRef.current;
    if (!element || typeof ResizeObserver === 'undefined') return undefined;
    const updateSize = () => {
      const bounds = element.getBoundingClientRect();
      setWorkspaceSize({ width: bounds.width, height: bounds.height });
    };
    updateSize();
    const observer = new ResizeObserver(updateSize);
    observer.observe(element);
    return () => observer.disconnect();
  }, [resolvedLayoutId]);

  useEffect(() => {
    setPreviewRatio(undefined);
    setResizing(false);
  }, [primaryRoomId, resolvedLayoutId, rooms.length]);

  return (
    <main className="workspace-main">
      {rooms.length ? (
        <div
          ref={gridRef}
          className={`workspace-grid layout-${resolvedLayoutId} ${focusPlan && focusPlan.secondaryRows > 0 ? `has-primary-divider primary-orientation-${focusPlan.orientation}` : ''} ${resizing ? 'is-resizing' : ''}`}
          style={gridStyle}
        >
          {rooms.map((room, index) => {
            const slot = slotsByRoomId.get(room.roomId);
            return slot ? (
              <RoomTile
                room={room}
                slot={slot}
                index={index}
                controlsLocked={resizing && room.roomId === primaryRoomId}
                key={room.roomId}
              />
            ) : null;
          })}
          {focusPlan && focusPlan.secondaryRows > 0 ? (
            <PrimaryRoomDivider
              orientation={focusPlan.orientation}
              value={activeRatio}
              availableRatios={focusPlan.availableRatios}
              onPreviewChange={setPreviewRatio}
              onCommit={setPrimaryRoomRatio}
              onDragStateChange={setResizing}
            />
          ) : null}
        </div>
      ) : (
        <div className="workspace-empty">
          <div className="empty-illustration"><Sparkles size={25} /></div>
          <h3>把直播间放进同一张画布</h3>
          <p>用房间号或主播名字添加第一路信号，弹幕会叠加在对应画面上。</p>
          <button className="button button-primary" type="button" onClick={onAddRoom}><Plus size={16} />添加第一路直播</button>
        </div>
      )}
    </main>
  );
}
