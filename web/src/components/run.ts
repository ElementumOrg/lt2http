import { notifications } from '@mantine/notifications';

// Runs an API call of a button: shows its error, or the done message, then calls after()
export async function run(action: () => Promise<unknown>, done?: string, after?: () => void): Promise<void> {
  try {
    await action();
    if (done) notifications.show({ message: done, color: 'green' });
    after?.();
  } catch (e) {
    notifications.show({ message: (e as Error).message, color: 'red' });
  }
}
