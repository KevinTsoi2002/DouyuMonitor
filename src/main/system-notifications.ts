import { Notification } from 'electron';

export interface SystemNotificationInput {
  title: string;
  body: string;
}

export interface SystemNotificationService {
  isSupported(): boolean;
  show(input: SystemNotificationInput): Promise<void>;
}

export function createSystemNotificationService(): SystemNotificationService {
  return {
    isSupported() {
      return Notification.isSupported();
    },
    async show(input) {
      new Notification({ title: input.title, body: input.body }).show();
    },
  };
}
