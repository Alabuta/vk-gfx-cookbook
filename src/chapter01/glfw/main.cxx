import vkgc.bootstrap;
import vkgc.window;

int main()
{
    vkgc::bootstrap_app();

    {
        vkgc::window const window{"GLFW example", 1280, 800};
        if (!window.is_valid())
        {
            return -1;
        }

        while (!window.should_close())
        {
            vkgc::update_loop(nullptr);
        }
    }

    vkgc::terminate_app();
}
