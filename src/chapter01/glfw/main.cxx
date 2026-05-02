import cookbook.bootstrap;
import cookbook.window;

int main()
{
    if (!cookbook::bootstrap_app())
    {
        return -1;
    }

    {
        cookbook::window const window{"GLFW example", 1280, 800};
        if (!window)
        {
            return -1;
        }

        while (!window.should_close())
        {
            cookbook::tick_app(nullptr);
        }
    }

    cookbook::terminate_app();
}
