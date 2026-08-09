import { Plus, Sparkles } from 'lucide-react';
import { calculateLayout, resolveLayoutId } from '../../domain/layout-engine';
import { useWorkspace } from '../store/workspace-context';
import { RoomTile } from './RoomTile';

interface WorkspaceGridProps {
  onAddRoom: () => void;
}

export function WorkspaceGrid({ onAddRoom }: WorkspaceGridProps) {
  const rooms = useWorkspace((state) => state.rooms);
  const layoutId = useWorkspace((state) => state.layoutId);
  const primaryRoomId = useWorkspace((state) => state.primaryRoomId);
  const resolvedLayoutId = resolveLayoutId(layoutId, rooms.length);
  const slots = calculateLayout(rooms.map((room) => room.roomId), resolvedLayoutId, primaryRoomId);
  const roomsById = new Map(rooms.map((room) => [room.roomId, room]));

  return (
    <main className="workspace-main">
      {rooms.length ? (
        <div className={`workspace-grid layout-${resolvedLayoutId}`} style={{ '--room-count': rooms.length } as React.CSSProperties}>
          {slots.map((slot, index) => {
            const room = roomsById.get(slot.roomId);
            return room ? <RoomTile room={room} slot={slot} index={index} key={room.roomId} /> : null;
          })}
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
