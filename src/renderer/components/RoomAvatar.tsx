import { useState } from 'react';
import { getRoomInitials } from '../ui-model';

interface RoomAvatarProps {
  anchorName: string;
  avatarUrl?: string;
  size?: 'small' | 'medium';
}

export function RoomAvatar({
  anchorName,
  avatarUrl,
  size = 'medium',
}: RoomAvatarProps) {
  const [failedUrl, setFailedUrl] = useState<string>();
  const showImage = Boolean(avatarUrl && avatarUrl !== failedUrl);

  return (
    <span className={`room-avatar room-avatar-${size}`}>
      {showImage ? (
        <img
          src={avatarUrl}
          alt={anchorName}
          referrerPolicy="no-referrer"
          draggable={false}
          onError={() => setFailedUrl(avatarUrl)}
        />
      ) : (
        <span aria-hidden="true">{getRoomInitials(anchorName)}</span>
      )}
    </span>
  );
}
